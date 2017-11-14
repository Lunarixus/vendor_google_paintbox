//#define LOG_NDEBUG 0
#define LOG_TAG "HdrPlusPipeline"
#include <log/log.h>

#include <inttypes.h>
#include <fstream>
#include <sstream>
#include <system/graphics.h>

#include "blocks/HdrPlusProcessingBlock.h"
#include "blocks/SourceCaptureBlock.h"
#include "blocks/CaptureResultBlock.h"
#include "HdrPlusPipeline.h"

#include "third_party/halide/paintbox/src/runtime/imx.h"

namespace {
void readFile(const char* filename, std::vector<char>* out) {
    std::ifstream input(filename, std::ios::in | std::ios::binary);
    if (input) {
        *out = std::vector<char>(std::istreambuf_iterator<char>(input), {});
    } else {
        ALOGE("%s: Could not read file %s.", __FUNCTION__, filename);
    }
}
std::string getDumpProfileFileName() {
    int64_t now = 0;
    pbcamera::status_t res =
        EaselControlServer::getApSynchronizedClockBoottime(&now);
    if (res != 0) {
        ALOGE("%s: Couldn't read timestamp.", __FUNCTION__);
    }
    std::stringstream path;
    path << "timing_" << now << ".prof";
    return path.str();
}

const char* kProfileFile = "/tmp/profiler.timing";
}  // namespace

namespace pbcamera {

std::shared_ptr<HdrPlusPipeline> HdrPlusPipeline::newPipeline(
        std::shared_ptr<MessengerToHdrPlusClient> messengerToClient) {
    return std::shared_ptr<HdrPlusPipeline>(new HdrPlusPipeline(messengerToClient));
}

HdrPlusPipeline::HdrPlusPipeline(std::shared_ptr<MessengerToHdrPlusClient> messengerToClient) :
        mMessengerToClient(messengerToClient),
        mState(STATE_UNCONFIGURED),
        mImxMemoryAllocatorHandle(nullptr),
        mImxIpuDevice(nullptr),
        mProfilingEnabled(false) {
}

HdrPlusPipeline::~HdrPlusPipeline() {
    std::unique_lock<std::mutex> lock(mApiLock);

    // This is safe because mMessengerToClient is passed into HdrPlusPipeline's
    // constructor and thus is guranteed to outlive the HdrPlusPipeline.
    if (mProfilingEnabled) {
        ImxDisableProfiling();
        std::vector<char> data;
        readFile(kProfileFile, &data);
        mMessengerToClient->notifyFileDump(
            getDumpProfileFileName(),
            const_cast<void*>(static_cast<const void*>(&data[0])),
            /*dmaBufFd=*/-1, data.size());
    }
    destroyLocked();
}

status_t HdrPlusPipeline::setStaticMetadata(const StaticMetadata& metadata) {
    std::unique_lock<std::mutex> lock(mApiLock);
    if (mStaticMetadata != nullptr) {
        ALOGE("%s: Static metadata is already set.", __FUNCTION__);
        return -EINVAL;
    }

    IF_ALOGV() {
        std::string s;
        metadata.appendToString(&s);
        ALOGV("%s: static metadata: %s.", __FUNCTION__, s.data());
    }

    bool shouldSaveProfile =
        (metadata.debugParams & DEBUG_PARAM_SAVE_PROFILE) != 0;
    if (shouldSaveProfile) {
        if (ImxEnableProfiling(kProfileFile) != IMX_SUCCESS) {
            ALOGE("%s: Failed to start profiling on easel.", __FUNCTION__);
        } else {
            mProfilingEnabled = true;
        }
    }

    mStaticMetadata = std::make_shared<StaticMetadata>();
    *mStaticMetadata = metadata;

    return 0;
}

status_t HdrPlusPipeline::configure(const InputConfiguration &inputConfig,
            const std::vector<StreamConfiguration> &outputConfigs) {
    if (outputConfigs.size() == 0) {
        ALOGE("%s: There must be at least 1 output stream.", __FUNCTION__);
        return -EINVAL;
    }

    if (inputConfig.isSensorInput) {
        ALOGV("%s: Sensor input: pixelArray %dx%d, activeArray %dx%d, sensor output pixel clock %d",
                __FUNCTION__, inputConfig.sensorMode.pixelArrayWidth,
                inputConfig.sensorMode.pixelArrayHeight,
                inputConfig.sensorMode.activeArrayWidth,
                inputConfig.sensorMode.activeArrayHeight,
                inputConfig.sensorMode.outputPixelClkHz);
    } else {
        ALOGV("%s: AP Input: %dx%d %d", __FUNCTION__, inputConfig.streamConfig.image.width,
                inputConfig.streamConfig.image.height, inputConfig.streamConfig.image.format);
    }

    for (auto outputConfig : outputConfigs) {
        ALOGV("%s: Output: %dx%d %d", __FUNCTION__, outputConfig.image.width,
            outputConfig.image.height, outputConfig.image.format);
    }

    std::unique_lock<std::mutex> lock(mApiLock);

    // TODO: Check if we can avoid allocating unchanged streams again.
    destroyLocked();

    // Allocate pipeline streams.
    status_t res = createStreamsLocked(inputConfig, outputConfigs);
    if (res != 0) {
        ALOGE("%s: Configuring stream failed: %s (%d)", __FUNCTION__, strerror(-res), res);
        destroyLocked();
        return res;
    }

    // Set up routes for each stream.
    const SensorMode *sensorMode = inputConfig.isSensorInput ? &inputConfig.sensorMode : nullptr;
    res = createBlocksAndStreamRouteLocked(sensorMode);
    if (res != 0) {
        ALOGE("%s: Configuring pipeline route failed: %s (%d)", __FUNCTION__, strerror(-res), res);
        destroyLocked();
        return res;
    }

    // Now pipeline is configured, updated the state.
    mState = STATE_STOPPED;
    return 0;
}

status_t HdrPlusPipeline::setZslHdrPlusMode(bool enabled) {
    std::unique_lock<std::mutex> lock(mApiLock);
    if (enabled) {
        // Start running pipeline.
        switch (mState) {
            case STATE_UNCONFIGURED:
                ALOGE("%s: Pipeline is not configured.", __FUNCTION__);
                return -ENODEV;
            case STATE_RUNNING:
                return 0;
            case STATE_STOPPED:
                return startRunningPipelineLocked();
            case STATE_STOPPING:
                // This should not happen because STATE_STOPPING is a transient state in
                // stopPipelineLocked
                ALOGE("%s: Cannot enable ZSL HDR+ mode when pipeline is stopping.", __FUNCTION__);
                return -EINVAL;
            default:
                ALOGE("%s: Pipeline is in an unknown state: %d.", __FUNCTION__,
                        mState);
                return -EINVAL;
        }
    } else {
        // Stop the pipeline.
        switch (mState) {
            case STATE_UNCONFIGURED:
                ALOGE("%s: Pipeline is not configured.", __FUNCTION__);
                return -ENODEV;
            case STATE_RUNNING:
                return stopPipelineLocked();
            case STATE_STOPPED:
                return 0;
            case STATE_STOPPING:
                // This should not happen because STATE_STOPPING is a transient state in
                // stopPipelineLocked
                ALOGE("%s: Cannot disable ZSL HDR+ mode when pipeline is stopping.", __FUNCTION__);
                return -EINVAL;
            default:
                ALOGE("%s: Pipeline is in an unknown state: %d.", __FUNCTION__,
                        mState);
                return -EINVAL;
        }
    }

    return 0;
}

status_t HdrPlusPipeline::stopPipelineLocked() {
    ALOGV("%s", __FUNCTION__);
    mState = STATE_STOPPING;
    bool failed = false;

    // Re-order blocks so that HDR+ processing block will be stopped first because capture service
    // cannot be destroyed before HDR+ processing completes.
    std::deque<std::shared_ptr<PipelineBlock>> blocks;
    for (auto block : mBlocks) {
        if (block == mHdrPlusProcessingBlock) {
            blocks.push_front(block);
        } else {
            blocks.push_back(block);
        }
    }

    if (mSourceCaptureBlock != nullptr) {
        // Pause source capture first so make sure it won't run out of buffers while waiting for
        // HDR+ processing block to stop.
        std::static_pointer_cast<SourceCaptureBlock>(mSourceCaptureBlock)->pauseCapture();
    }

    // Wait until all blocks are stopped.
    for (auto block : blocks) {
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

    mState = STATE_STOPPED;

    ALOGI("%s: HDR+ pipeline is stopped.", __FUNCTION__);
    return 0;
}

status_t HdrPlusPipeline::startRunningPipelineLocked() {
    status_t res = 0;

    // Send all buffers in the input stream to its first block.
    PipelineBuffer *buffer = nullptr;
    while (mInputStream->getBuffer(&buffer, /*timeoutMs*/ 0) == 0) {
        PipelineBlock::OutputRequest outputRequest = {};
        outputRequest.buffers.push_back(buffer);
        outputRequest.route = mInputStreamRoute;
        auto block = getNextBlockLocked(outputRequest);
        if (block == nullptr) {
            ALOGE("%s: Could not find the starting block for input stream.",
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

    ALOGI("%s: HDR+ pipeline is running.", __FUNCTION__);
    return 0;
}

void HdrPlusPipeline::destroyLocked() {
    ALOGV("%s", __FUNCTION__);
    // Stop the pipeline.
    stopPipelineLocked();

    // Delete all streams.
    mInputStream = nullptr;
    mOutputStreams.clear();
    mInputStreamRoute.clear();
    mOutputStreamRoute.clear();

    // Delete all blocks.
    mBlocks.clear();
    mSourceCaptureBlock = nullptr;
    mHdrPlusProcessingBlock = nullptr;
    mCaptureResultBlock = nullptr;

    if (mImxMemoryAllocatorHandle != nullptr) {
        ImxError err = ImxDeleteMemoryAllocator(mImxMemoryAllocatorHandle);
        if (err != IMX_SUCCESS) {
            ALOGE("%s: Deleting ImxMemoryAllocator failed.", __FUNCTION__);
        }
        mImxMemoryAllocatorHandle = nullptr;
    }

    if (mImxIpuDevice != nullptr) {
        ImxError err = ImxDeleteDevice(mImxIpuDevice);
        if (err != IMX_SUCCESS) {
            ALOGE("%s: Deleting ImxIPUDevice failed.", __FUNCTION__);
        }
        mImxIpuDevice = nullptr;
    }

    mState = STATE_UNCONFIGURED;
}

status_t HdrPlusPipeline::createStreamsLocked(const InputConfiguration &inputConfig,
            const std::vector<StreamConfiguration> &outputConfigs) {

    if (mImxMemoryAllocatorHandle == nullptr) {
        ImxError err = ImxGetMemoryAllocator(IMX_MEMORY_ALLOCATOR_ION,
                &mImxMemoryAllocatorHandle);
        if (err != 0) {
            ALOGE("%s: Creating IMX memory allocator failed.", __FUNCTION__);
            return -ENOMEM;
        }
    }

    // TODO: Avoid recreate input stream if it has not changed. b/35673698
    mInputStream = PipelineStream::newInputPipelineStream(inputConfig, kDefaultNumInputBuffers);
    if (mInputStream == nullptr) {
        ALOGE("%s: Initialize input stream failed.", __FUNCTION__);
        return -ENODEV;
    }

    // TODO: Don't delete the output streams that have the same configuration as the new one.
    mOutputStreams.clear();

    // Allocate output streams.
    for (auto outputConfig : outputConfigs) {
        std::shared_ptr<PipelineStream> stream = PipelineStream::newPipelineStream(
                mImxMemoryAllocatorHandle, outputConfig, kDefaultNumOutputBuffers);
        if (stream == nullptr) {
            ALOGE("%s: Initialize output stream failed.", __FUNCTION__);
            return -ENODEV;
        }
        mOutputStreams.push_back(stream);
    }

    return 0;
}

status_t HdrPlusPipeline::createMockCapture(){
    ImxDeviceDescription deviceDescriptor{};
    deviceDescriptor.core_resource_description_mode = IMX_NO_RESOURCES_DESCRIPTION;
    deviceDescriptor.io_resource_description_mode = IMX_SPECIFIC_RESOURCES_DESCRIPTION;
    const int kFrontCameraStreamId = 4;
    const int kBackCameraStreamId = 0;
    const int mockCameraType = 1;
    int mipiInStreamId = mockCameraType ? kBackCameraStreamId : kFrontCameraStreamId;
    int mipiInStreamCount = 1;
    int * mipiOutStreams = nullptr;
    int mipiOutStreamCount = 0;
    deviceDescriptor.io_resource_description.specific_mipi_resources = {
            &mipiInStreamId, mipiInStreamCount, mipiOutStreams, mipiOutStreamCount};
    ImxError err = ImxGetDevice(&deviceDescriptor, &mImxIpuDevice);
    if (err != IMX_SUCCESS) {
        ALOGE("%s: Getting IMX device failed.", __FUNCTION__);
        return -ENODEV;
    }

    return 0;
}

status_t HdrPlusPipeline::createBlocksAndStreamRouteLocked(const SensorMode *sensorMode) {
    // Create a mock version of capture service if data is from client side
    if (sensorMode == nullptr) {
        status_t res = createMockCapture();
        if (res != 0) {
            ALOGE("%s: Creating Mock Capture failed.", __FUNCTION__);
            return -ENODEV;
        }
    }

    // Create an source capture block for capturing input streams.
    std::shared_ptr<SourceCaptureBlock> sourceCaptureBlock =
            SourceCaptureBlock::newSourceCaptureBlock(shared_from_this(), mMessengerToClient,
            sensorMode);
    mSourceCaptureBlock = sourceCaptureBlock;
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

    int32_t cameraId = sensorMode == nullptr ? -1 : sensorMode->cameraId;

    // Create an HDR+ processing block for HDR+ processing.
    mHdrPlusProcessingBlock = HdrPlusProcessingBlock::newHdrPlusProcessingBlock(shared_from_this(),
            mStaticMetadata, sourceCaptureBlock, sensorMode == nullptr, cameraId,
            mImxMemoryAllocatorHandle, mMessengerToClient);
    if (mHdrPlusProcessingBlock == nullptr) {
        ALOGE("%s: Creating HdrPlusProcessingBlock failed.", __FUNCTION__);
        return -ENODEV;
    }
    mBlocks.push_back(mHdrPlusProcessingBlock);

    // Set up the routes for each stream.
    // Route for input stream: SourceCaptureBlock -> HdrPlusProcessingBlock.
    mInputStreamRoute.clear();
    mInputStreamRoute.blocks.push_back(mSourceCaptureBlock);
    mInputStreamRoute.blocks.push_back(mHdrPlusProcessingBlock);
    // Input stream route is circular so the input buffers go back to be captured for next frame.
    mInputStreamRoute.isCircular = true;

    // Route for output streams: HdrPlusProcessingBlock -> CaptureResultBlock
    mOutputStreamRoute.clear();
    mOutputStreamRoute.blocks.push_back(mHdrPlusProcessingBlock);
    mOutputStreamRoute.blocks.push_back(mCaptureResultBlock);

    return 0;
}

void HdrPlusPipeline::abortRequest(PipelineBlock::OutputRequest *outputRequest) {
    if (outputRequest == nullptr) return;
    returnBufferToStream(outputRequest->buffers);
}

status_t HdrPlusPipeline::submitCaptureRequest(const CaptureRequest &request,
        const RequestMetadata &metadata) {
    ALOGV("%s", __FUNCTION__);
    status_t res = 0;

    std::unique_lock<std::mutex> lock(mApiLock);

    // Prepare outputRequest
    PipelineBlock::OutputRequest outputRequest = {};
    outputRequest.metadata.requestId = request.id;
    outputRequest.metadata.requestMetadata = std::make_shared<RequestMetadata>();
    *outputRequest.metadata.requestMetadata = metadata;

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
                }

                ALOGV("%s: Requesting %dx%d format %d.", __FUNCTION__, buffer->getWidth(),
                        buffer->getHeight(), buffer->getFormat());
                outputRequest.buffers.push_back(buffer);
            }
        }
    }

    // Check if we got all output buffers.
    if (request.outputBuffers.size() != outputRequest.buffers.size()) {
        ALOGE("%s: Failed to get all buffers for request %d", __FUNCTION__, request.id);
        abortRequest(&outputRequest);
        return -EINVAL;
    }

    if (!std::static_pointer_cast<HdrPlusProcessingBlock>(mHdrPlusProcessingBlock)->isReady()) {
        ALOGE("%s: HDR+ processing block not ready for request %d", __FUNCTION__, request.id);
        abortRequest(&outputRequest);
        return -EINVAL;
    }

    outputRequest.route = mOutputStreamRoute;
    std::shared_ptr<PipelineBlock> startingBlock = getNextBlockLocked(outputRequest);
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

std::shared_ptr<PipelineBlock> HdrPlusPipeline::getNextBlockLocked(
        const PipelineBlock::BlockIoData &blockData) {
    std::shared_ptr<PipelineBlock> nextBlock;

    int32_t currentBlockIndex = blockData.route.currentBlockIndex;

    // Sanity check the route and current block.
    if (blockData.route.blocks.size() <= 0) {
        // Return nullptr if route is empty.
        ALOGE("%s: route doesn't contain any blocks.", __FUNCTION__);
        return nullptr;
    } else if (currentBlockIndex >= static_cast<int32_t>(blockData.route.blocks.size()) ||
               currentBlockIndex < -1) {
        ALOGE("%s: Current block index (%d) is out of range (route size %zu).", __FUNCTION__,
                currentBlockIndex, blockData.route.blocks.size());
        return nullptr;
    }

    // Currently in the last block?
    if (currentBlockIndex == static_cast<int32_t>(blockData.route.blocks.size()) - 1) {
        // If it's a circular route, return the first block. Otherwise, return nullptr.
        return blockData.route.isCircular ? blockData.route.blocks[0] : nullptr;
    }

    // Return the next block in the route.
    return blockData.route.blocks[currentBlockIndex + 1];
}

void HdrPlusPipeline::inputDone(PipelineBlock::Input input) {
    if (mState != STATE_RUNNING) {
        // If pipeline is not running, return buffers back to streams.
        returnBufferToStream(input.buffers);
        return;
    }

    // Figure out where the input buffer goes.
    std::shared_ptr<PipelineBlock> nextBlock = getNextBlockLocked(input);
    if (nextBlock == nullptr) {
        // Return all buffers to streams.
        returnBufferToStream(input.buffers);
    } else {
        // Send the buffer to next block. This should send the input stream buffers back to the
        // first block to be filled.
        PipelineBlock::OutputRequest output = input;
        output.route.resetCurrentBlock();
        status_t res = nextBlock->queueOutputRequest(&output);
        if (res != 0) {
            ALOGE("%s: Queueing an output to %s failed: %s (%d). Returning buffers to streams",
                    __FUNCTION__, nextBlock->getName(), strerror(-res), res);
            returnBufferToStream(input.buffers);
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
    if (mState != STATE_RUNNING) {
        // If pipeline is not running, return buffers back to streams.
        returnBufferToStream(outputResult.buffers);
        return;
    }

    std::shared_ptr<PipelineBlock> nextBlock = getNextBlockLocked(outputResult);
    if (nextBlock == nullptr) {
        // Return all buffers to streams.
        returnBufferToStream(outputResult.buffers);
    } else {
        // Send the buffer to next block. This assumes that output of a block becomes the input of
        // the next block. This is true for all current use case.
        PipelineBlock::Input input = outputResult;
        status_t res = nextBlock->queueInput(&input);
        if (res != 0) {
            ALOGE("%s: Queueing an input to %s failed: %s (%d). Returning buffers to streams",
                    __FUNCTION__, nextBlock->getName(), strerror(-res), res);
            returnBufferToStream(outputResult.buffers);
        }
    }
}

void HdrPlusPipeline::abortBlockIoData(PipelineBlock::BlockIoData *data) {
    if (data == nullptr) {
        ALOGE("%s: data is nullptr.", __FUNCTION__);
        return;
    }

    // If the pipeline is running and the data route is circular, queue the data to the first block.
    // Otherwise, return the buffers to streams.
    if (mState == STATE_RUNNING && data->route.isCircular) {
        data->route.resetCurrentBlock();
        // We don't need to hold mApiLock here because when pipeline state is running, the pipeline
        // is guaranteed to be valid.
        std::shared_ptr<PipelineBlock> block = getNextBlockLocked(*data);
        status_t res = block->queueOutputRequest(data);
        if (res != 0) {
            ALOGE("%s: Queueing an output request to %s failed: %s (%d). Returning buffers to "
                "streams", __FUNCTION__, block->getName(), strerror(-res), res);
            returnBufferToStream(data->buffers);
        }
    } else {
        returnBufferToStream(data->buffers);
    }

    return;
}

void HdrPlusPipeline::inputAbort(PipelineBlock::Input input) {
    abortBlockIoData(&input);
}

void HdrPlusPipeline::outputRequestAbort(PipelineBlock::OutputRequest outputRequest) {
    abortBlockIoData(&outputRequest);
}

} // pbcamera
