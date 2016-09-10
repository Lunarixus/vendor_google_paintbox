//#define LOG_NDEBUG 0
#define LOG_TAG "HdrPlusPipeline"
#include "Log.h"

#include <inttypes.h>

#include "blocks/HdrPlusProcessingBlock.h"
#include "blocks/SourceCaptureBlock.h"
#include "blocks/CaptureResultBlock.h"
#include "HdrPlusPipeline.h"

namespace pbcamera {

std::shared_ptr<HdrPlusPipeline> HdrPlusPipeline::newPipeline(
        std::shared_ptr<MessengerToHdrPlusClient> messengerToClient) {
    return std::shared_ptr<HdrPlusPipeline>(new HdrPlusPipeline(messengerToClient));
}

HdrPlusPipeline::HdrPlusPipeline(std::shared_ptr<MessengerToHdrPlusClient> messengerToClient) :
        mMessengerToClient(messengerToClient),
        mState(STATE_UNCONFIGURED) {
}

HdrPlusPipeline::~HdrPlusPipeline() {
    std::unique_lock<std::mutex> lock(mApiLock);
    destroyLocked();
}

status_t HdrPlusPipeline::setStaticMetadata(const StaticMetadata& metadata) {
    std::unique_lock<std::mutex> lock(mApiLock);
    if (mStaticMetadata != nullptr) {
        ALOGE("%s: Static metadata is already set.", __FUNCTION__);
        return -EINVAL;
    }

    mStaticMetadata = std::make_shared<StaticMetadata>();
    *mStaticMetadata = metadata;

    return 0;
}

status_t HdrPlusPipeline::configure(const StreamConfiguration &inputConfig,
            const std::vector<StreamConfiguration> &outputConfigs) {
    if (outputConfigs.size() == 0) {
        ALOGE("%s: There must be at least 1 output stream.", __FUNCTION__);
        return -EINVAL;
    }

    ALOGV("%s: Input: %dx%d %d", __FUNCTION__, inputConfig.width,
            inputConfig.height, inputConfig.format);
    for (auto outputConfig : outputConfigs) {
        ALOGV("%s: Output: %dx%d %d", __FUNCTION__, outputConfig.width,
            outputConfig.height, outputConfig.format);
    }

    std::unique_lock<std::mutex> lock(mApiLock);
    status_t res = stopPipelineLocked();
    if (res != 0) {
        ALOGE("%s: Stopping pipeline failed: %s (%d).", __FUNCTION__, strerror(-res), res);
        return -ENODEV;
    }

    // TODO: Check if we can avoid allocating unchanged streams again.
    destroyLocked();

    // Allocate pipeline streams.
    res = createStreamsLocked(inputConfig, outputConfigs);
    if (res != 0) {
        ALOGE("%s: Configuring stream failed: %s (%d)", __FUNCTION__, strerror(-res), res);
        destroyLocked();
        return res;
    }

    // Set up routes for each stream.
    res = createBlocksAndStreamRouteLocked();
    if (res != 0) {
        ALOGE("%s: Configuring pipeline route failed: %s (%d)", __FUNCTION__, strerror(-res), res);
        destroyLocked();
        return res;
    }

    // Now pipeline is configured, updated the state.
    mState = STATE_STOPPED;

    // Start running the pipeline.
    res = startRunningPipelineLocked();
    if (res != 0) {
        ALOGE("%s: Starting running pipeline failed: %s (%d)", __FUNCTION__, strerror(-res), res);
        destroyLocked();
        return -ENODEV;
    }

    return 0;
}

status_t HdrPlusPipeline::stopPipelineLocked() {
    ALOGV("%s", __FUNCTION__);
    mState = STATE_STOPPING;
    bool failed = false;

    // Wait until all blocks are stopped.
    for (auto block : mBlocks) {
        ALOGV("%s: Stopping %s.", __FUNCTION__, block->getName());
        status_t res = block->stopAndFlush(kStopBlockTimeoutMs);
        if (res != 0) {
            ALOGE("%s: Stopping %s failed: %s (%d).", __FUNCTION__, block->getName(),
                    strerror(-res), res);
            failed = true;
        }
    }

    if (failed) {
        return -ENODEV;
    }

    ALOGV("%s: All blocks stopped.", __FUNCTION__);

    mState = STATE_STOPPED;
    return 0;
}

status_t HdrPlusPipeline::startRunningPipelineLocked() {
    switch (mState) {
        case STATE_UNCONFIGURED:
            ALOGE("%s: Pipeline is not configured.", __FUNCTION__);
            return -ENODEV;
        case STATE_RUNNING:
            return 0;
        case STATE_STOPPED:
            // Start the pipeline.
            {
                status_t res = 0;

                // Send all buffers in the input stream to its first block.
                PipelineBuffer *buffer = nullptr;
                while (mInputStream->getBuffer(&buffer, /*timeoutMs*/ 0) == 0) {
                    PipelineBlock::OutputRequest outputRequest = {};
                    outputRequest.buffers.push_back(buffer);
                    auto block = getNextBlockLocked(*buffer);
                    if (block == nullptr) {
                        ALOGE("%s: Could not find the starting block for the output buffers.",
                                __FUNCTION__);
                        mInputStream->returnBuffer(buffer);
                        return -ENOENT;
                    }

                    res = block->queueOutputRequest(&outputRequest);
                    if (res != 0) {
                        ALOGE("%s: Couldn't queue a request to %s: %s (%d).", __FUNCTION__,
                                block->getName(), strerror(-res), res);
                        abortRequest(&outputRequest);
                        return res;
                    }
                }

                // Set the pipeline state to running before running blocks because blocks can
                // start sending buffers back immedidately.
                mState = STATE_RUNNING;

                // Start running all blocks.
                for (auto block : mBlocks) {
                    res = block->run();
                    if (res != 0) {
                        ALOGE("%s: Starting block %s failed: %s (%d).", __FUNCTION__,
                                block->getName(), strerror(-res), res);
                        stopPipelineLocked();
                        return res;
                    }
                }

                return 0;
            }
            break;
        default:
            ALOGE("%s: Pipeline is in an unknown state: %d.", __FUNCTION__,
                    mState);
            return -EINVAL;
    }
}

void HdrPlusPipeline::destroyLocked() {
    ALOGV("%s", __FUNCTION__);
    // Stop the pipeline.
    stopPipelineLocked();

    // Delete all streams.
    mInputStream = nullptr;
    mOutputStreams.clear();
    mStreamRoutes.clear();

    // Delete all blocks.
    mBlocks.clear();
    mSourceCaptureBlock = nullptr;
    mHdrPlusProcessingBlock = nullptr;
    mCaptureResultBlock = nullptr;

    mState = STATE_UNCONFIGURED;
}

status_t HdrPlusPipeline::createStreamsLocked(const StreamConfiguration &inputConfig,
            const std::vector<StreamConfiguration> &outputConfigs) {
    // Allocate input stream if it doesn't exist or changed.
    if (mInputStream == nullptr || !mInputStream->hasConfig(inputConfig)) {
        mInputStream = PipelineStream::newPipelineStream(inputConfig, kDefaultNumInputBuffers);
        if (mInputStream == nullptr) {
            ALOGE("%s: Initialize input stream failed.", __FUNCTION__);
            return -ENODEV;
        }
    }

    // TODO: Don't delete the output streams that have the same configuration as the new one.
    mOutputStreams.clear();

    // Allocate output streams.
    for (auto outputConfig : outputConfigs) {
        std::shared_ptr<PipelineStream> stream = PipelineStream::newPipelineStream(outputConfig,
                kDefaultNumOutputBuffers);
        if (stream == nullptr) {
            ALOGE("%s: Initialize output stream failed.", __FUNCTION__);
            return -ENODEV;
        }
        mOutputStreams.push_back(stream);
    }

    return 0;
}

status_t HdrPlusPipeline::createBlocksAndStreamRouteLocked() {
    // Create an source capture block for capturing input streams.
    mSourceCaptureBlock = SourceCaptureBlock::newSourceCaptureBlock(shared_from_this(),
            mMessengerToClient);
    if (mSourceCaptureBlock == nullptr) {
        ALOGE("%s: Creating SourceCaptureBlock failed.", __FUNCTION__);
        return -ENODEV;
    }
    mBlocks.push_back(mSourceCaptureBlock);

    // Create an capture result block for sending capture results to the client.
    mCaptureResultBlock = CaptureResultBlock::newCaptureResultBlock(shared_from_this(),
            mMessengerToClient);
    if (mCaptureResultBlock == nullptr) {
        ALOGE("%s: Creating CaptureResultBlock failed.", __FUNCTION__);
        return -ENODEV;
    }
    mBlocks.push_back(mCaptureResultBlock);

    // Create an HDR+ processing block for HDR+ processing.
    mHdrPlusProcessingBlock = HdrPlusProcessingBlock::newHdrPlusProcessingBlock(shared_from_this(),
            mStaticMetadata);
    if (mHdrPlusProcessingBlock == nullptr) {
        ALOGE("%s: Creating HdrPlusProcessingBlock failed.", __FUNCTION__);
        return -ENODEV;
    }
    mBlocks.push_back(mHdrPlusProcessingBlock);

    // Set up the routes for each stream.
    mStreamRoutes.clear();
    StreamRoute route;

    // Route for input stream: SourceCaptureBlock -> HdrPlusProcessingBlock
    route.push_back(mSourceCaptureBlock);
    route.push_back(mHdrPlusProcessingBlock);
    mStreamRoutes.emplace(mInputStream, route);

    // Route for output streams: HdrPlusProcessingBlock -> CaptureResultBlock
    for (auto stream : mOutputStreams) {
        route.clear();
        route.push_back(mHdrPlusProcessingBlock);
        route.push_back(mCaptureResultBlock);
        mStreamRoutes.emplace(stream, route);
    }

    return 0;
}

void HdrPlusPipeline::abortRequest(PipelineBlock::OutputRequest *outputRequest) {
    if (outputRequest == nullptr) return;
    returnBufferToStream(outputRequest->buffers);
}

status_t HdrPlusPipeline::submitCaptureRequest(const CaptureRequest &request) {
    ALOGV("%s", __FUNCTION__);
    status_t res = 0;

    std::unique_lock<std::mutex> lock(mApiLock);

    // Prepare outputRequest
    PipelineBlock::OutputRequest outputRequest = {};
    outputRequest.metadata.requestId = request.id;

    // Find all output buffers.
    for (auto bufferInRequest : request.outputBuffers) {
        for (auto stream : mOutputStreams) {
            if (stream->getStreamId() == (int)bufferInRequest.streamId) {
                PipelineBuffer *buffer;
                res = stream->getBuffer(&buffer, kGetBufferTimeoutMs);
                if (res != 0) {
                    ALOGE("%s: Couldn't get a buffer for stream %d: %s (%d).", __FUNCTION__,
                            bufferInRequest.streamId, strerror(-res), res);
                    abortRequest(&outputRequest);
                    return -EINVAL;
                } else {
                    outputRequest.buffers.push_back(buffer);
                }
            }
        }
    }

    // Check if we got all output buffers.
    if (request.outputBuffers.size() != outputRequest.buffers.size()) {
        ALOGE("%s: Failed to get all buffers for request.", __FUNCTION__);
        abortRequest(&outputRequest);
        return -EINVAL;
    }

    std::shared_ptr<PipelineBlock> startingBlock;

    // Check all output buffers start at the same block.
    for (auto buffer : outputRequest.buffers) {
        auto block = getNextBlockLocked(*buffer);
        if (startingBlock == nullptr) {
            startingBlock = block;
        } else if (startingBlock != block) {
            // Pipeline doesn't support capture requests that have output buffers starting
            // at different blocks yet. We may want to support that when a new use case needs
            // it.
            ALOGE("%s: Not all output buffers start at the same block.", __FUNCTION__);
            abortRequest(&outputRequest);
            return -EINVAL;
        }
    }

    if (startingBlock == nullptr) {
        ALOGE("%s: Could not find the starting block for the output buffers.", __FUNCTION__);
        abortRequest(&outputRequest);
        return -EINVAL;
    }

    res = startingBlock->queueOutputRequest(&outputRequest);
    if (res != 0) {
        ALOGE("%s: Could not queue an output request to block: %s.", __FUNCTION__,
                startingBlock->getName());
        abortRequest(&outputRequest);
        return res;
    }

    return 0;
}

void HdrPlusPipeline::notifyDmaInputBuffer(const DmaImageBuffer &dmaInputBuffer,
        int64_t mockingEaselTimestampNs) {
    ALOGV("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mApiLock);
    if (mState != STATE_RUNNING) {
        ALOGE("%s: Pipeline is not running (state=%d). Dropping this input buffer.",
                __FUNCTION__, mState);
        return;
    }

    // Notify source capture block of the DMA input buffer.
    std::static_pointer_cast<SourceCaptureBlock>(mSourceCaptureBlock)->notifyDmaInputBuffer(
            dmaInputBuffer, mockingEaselTimestampNs);
}

void HdrPlusPipeline::notifyFrameMetadata(const FrameMetadata &metadata) {
    ALOGV("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mApiLock);
    if (mState != STATE_RUNNING) {
        ALOGE("%s: Pipeline is not running (state=%d). Dropping this input buffer.",
                __FUNCTION__, mState);
        return;
    }

    // Notify source capture block of the frame metadata.
    std::static_pointer_cast<SourceCaptureBlock>(mSourceCaptureBlock)->notifyFrameMetadata(
            metadata);
}

std::shared_ptr<PipelineBlock> HdrPlusPipeline::getNextBlockLocked(const PipelineBuffer &buffer) {
    std::shared_ptr<PipelineBlock> currentBlock = buffer.getPipelineBlock().lock();

    // Find the route of the buffer's stream.
    std::shared_ptr<PipelineStream> stream = buffer.getStream().lock();
    if (stream == nullptr) {
        ALOGE("%s: Stream has been destroyed.", __FUNCTION__);
        return nullptr;
    }

    auto iter = mStreamRoutes.find(stream);
    if (iter == mStreamRoutes.end()) {
        ALOGE("%s: Couldn't find the stream in stream routes.", __FUNCTION__);
        return nullptr;
    }

    // Return the first block in the route if the buffer is not currently in any block.
    StreamRoute &route = iter->second;
    if (currentBlock == nullptr && route.size() > 0) {
        return route[0];
    }

    // Return the next block in the route.
    for (uint32_t i = 0; i < route.size(); i++) {
        if (route[i] == currentBlock && i < route.size() - 1) {
            return route[i + 1];
        }
    }

    return nullptr;
}

void HdrPlusPipeline::inputDone(PipelineBlock::Input input) {
    // Figure out where the input buffer goes.
    for (auto buffer : input.buffers) {
        auto nextBlock = getNextBlockLocked(*buffer);
        if (nextBlock != nullptr && mState == STATE_RUNNING) {
            ALOGE("%s: Reusing input buffer is not supported. Return the buffer to stream.",
                    __FUNCTION__);
        }

        std::shared_ptr<PipelineStream> stream = buffer->getStream().lock();
        if (stream == nullptr) {
            ALOGE("%s: Stream has been destroyed.", __FUNCTION__);
        } else {
            stream->returnBuffer(buffer);
        }
    }

    (void)input;
}

void HdrPlusPipeline::returnBufferToStream(const PipelineBlock::PipelineBufferSet &buffers) {
    status_t res = 0;
    for (auto buffer : buffers) {
        std::shared_ptr<PipelineStream> stream = buffer->getStream().lock();
        if (stream == nullptr) {
            ALOGE("%s: Stream has been destroyed.", __FUNCTION__);
        } else {
            res = stream->returnBuffer(buffer);
            if (res != 0) {
                ALOGE("%s: Return a buffer %p to stream %p failed: %s (%d).", __FUNCTION__, buffer,
                    stream.get(), strerror(-res), res);
            }
        }
    }
}

void HdrPlusPipeline::outputDone(PipelineBlock::OutputResult outputResult) {
    std::shared_ptr<PipelineBlock> nextBlock = nullptr;

    // If the pipeline is running, search the stream route to find the next block.
    if (mState == STATE_RUNNING) {
        for (auto buffer : outputResult.buffers) {
            auto block = getNextBlockLocked(*buffer);
            if (nextBlock != nullptr && nextBlock != block) {
                // Forking outputs are not needed in current use case so it's not implemented.
                ALOGE("%s: Forking outputs are not supported. Returning all buffers to streams.",
                        __FUNCTION__);
                nextBlock = nullptr;
                break;
            } else if (nextBlock == nullptr) {
                nextBlock = block;
            }
        }
    }

    if (nextBlock == nullptr) {
        // Return all buffers to streams.
        returnBufferToStream(outputResult.buffers);
    } else {
        // Send the buffer to next block. This assumes that output of a block becomes the input of
        // the next block. This is true for all current use case.
        status_t res = nextBlock->queueInput(&outputResult);
        if (res != 0) {
            ALOGE("%s: Queueing an input to %s failed: %s (%d). Returning buffers to streams",
                    __FUNCTION__, nextBlock->getName(), strerror(-res), res);
            returnBufferToStream(outputResult.buffers);
        }
    }
}

void HdrPlusPipeline::inputAbort(PipelineBlock::Input input) {
    returnBufferToStream(input.buffers);
}

void HdrPlusPipeline::outputRequestAbort(PipelineBlock::OutputRequest outputRequest) {
    returnBufferToStream(outputRequest.buffers);
}

} // pbcamera
