//#define LOG_NDEBUG 0
#define LOG_TAG "SourceCaptureBlock"
#include "Log.h"

#include <inttypes.h>
#include <system/graphics.h>

#include <easelcontrol.h>

#include "CaptureServiceConsts.h"
#include "SourceCaptureBlock.h"
#include "HdrPlusPipeline.h"

namespace pbcamera {

static void dequeueRequestThread(SourceCaptureBlock* block) {
    if (block != nullptr) {
        block->dequeueRequestThreadLoop();
    }
}

void SourceCaptureBlock::dequeueRequestThreadLoop() {
    mDequeueRequestThread->dequeueRequestThreadLoop();
}

SourceCaptureBlock::SourceCaptureBlock(std::shared_ptr<MessengerToHdrPlusClient> messenger,
        std::unique_ptr<CaptureService> captureService) :
        PipelineBlock("SourceCaptureBlock"),
        mMessengerToClient(messenger),
        mCaptureService(std::move(captureService)) {
    if (mCaptureService != nullptr) {
        // Create a dequeue request thread.
        mDequeueRequestThread = std::make_unique<DequeueRequestThread>(this);
    }
}

SourceCaptureBlock::~SourceCaptureBlock(){
}

std::shared_ptr<SourceCaptureBlock> SourceCaptureBlock::newSourceCaptureBlock(
        std::weak_ptr<HdrPlusPipeline> pipeline,
        std::shared_ptr<MessengerToHdrPlusClient> messenger,
        const SensorMode *sensorMode) {

    if (sensorMode != nullptr && sensorMode->format != HAL_PIXEL_FORMAT_RAW10) {
        ALOGE("%s: Only RAW10 input is supported but format is %d", __FUNCTION__,
                sensorMode->format);
        return nullptr;
    }

    std::unique_ptr<CaptureService> captureService;

    if (sensorMode != nullptr) {
        uint32_t dataType = 0, bitsPerPixel = 0;
        switch (sensorMode->format) {
            case HAL_PIXEL_FORMAT_RAW10:
                // TODO: Replace with a macro once defined in capture.h.
                dataType = capture_service_consts::kMipiRaw10DataType;
                bitsPerPixel = 10;
                break;
            default:
                ALOGE("%s: Only HAL_PIXEL_FORMAT_RAW10 is supported but sensor mode has %d",
                        __FUNCTION__, sensorMode->format);
                return nullptr;
        }

        // Create a capture service.
        std::vector<CaptureStreamConfig> captureStreamConfigs;
        captureStreamConfigs.push_back({ dataType, sensorMode->pixelArrayWidth,
                sensorMode->pixelArrayHeight, bitsPerPixel});
        CaptureConfig config = { MipiRxPort::RX0,
                capture_service_consts::kMainImageVirtualChannelId,
                capture_service_consts::kCaptureFrameBufferFactoryTimeoutMs,
                captureStreamConfigs };

        captureService = CaptureService::CreateInstance(config);
        CaptureError err = captureService->Initialize();
        if (err != CaptureError::SUCCESS) {
            ALOGE("%s: Initializing capture service failed: %s (%d)", __FUNCTION__,
                    GetCaptureErrorDesc(err).data(), err);
            return nullptr;
        }
    }

    auto block = std::shared_ptr<SourceCaptureBlock>(new SourceCaptureBlock(messenger,
            std::move(captureService)));
    if (block == nullptr) {
        ALOGE("%s: Failed to create a block instance.", __FUNCTION__);
        return nullptr;
    }

    status_t res = block->create(pipeline);
    if (res != 0) {
        ALOGE("%s: Failed to create block %s", __FUNCTION__, block->getName());
        return nullptr;
    }

    return block;
}

bool SourceCaptureBlock::doWorkLocked() {
    ALOGV("%s", __FUNCTION__);

    // For input buffers coming from the client via notifyDmaInputBuffer(), there is nothing to do
    // here.
    if (mCaptureService == nullptr) return false;

    // Check if we have any output request
    OutputRequest outputRequest = {};
    {
        std::unique_lock<std::mutex> lock(mQueueLock);
        if (mOutputRequestQueue.size() == 0) {
            // Nothing to do this time.
            ALOGV("%s: No output request", __FUNCTION__);
            return false;
        }

        outputRequest = mOutputRequestQueue[0];
        mOutputRequestQueue.pop_front();

        // Make sure there is only 1 output buffer in the request.
        if (outputRequest.buffers.size() != 1) {
            ALOGE("%s: The request has %d output buffers but only 1 output buffer is supported.",
                    __FUNCTION__, (int)outputRequest.buffers.size());
            abortOutputRequest(outputRequest);
            return true;
        }
    }

    ALOGV("%s: Enqueue a request to capture service.", __FUNCTION__);

    // Enqueue a request to capture service to capture a frame from MIPI.
    PipelineCaptureFrameBuffer* pipelineBuffer =
            static_cast<PipelineCaptureFrameBuffer*>(outputRequest.buffers[0]);
    mCaptureService->EnqueueRequest(pipelineBuffer->getCaptureFrameBuffer());

    // Add the pending request to dequeue request thread.
    mDequeueRequestThread->addPendingRequest(outputRequest);

    return true;
}

void SourceCaptureBlock::notifyDmaInputBuffer(const DmaImageBuffer &dmaInputBuffer,
        int64_t mockingEaselTimestampNs) {
    ALOGV("%s", __FUNCTION__);

    OutputRequest outputRequest;

    {
        std::unique_lock<std::mutex> lock(mQueueLock);
        if (mOutputRequestQueue.size() == 0) {
            ALOGE("%s: No output request available.. Dropping this input buffer.", __FUNCTION__);
            return;
        }

        outputRequest = mOutputRequestQueue[0];

        // Make sure there is only 1 output buffer in the request.
        if (outputRequest.buffers.size() != 1) {
            ALOGE("%s: The request has %d output buffers but only 1 output buffer is supported.",
                    __FUNCTION__, (int)outputRequest.buffers.size());
            mOutputRequestQueue.pop_front();
            abortOutputRequest(outputRequest);
            return;
        }

        auto stream = outputRequest.buffers[0]->getStream().lock();
        if (stream == nullptr) {
            ALOGE("%s: Buffer's stream is destroyed.", __FUNCTION__);
            return;
        }

        // Check if the stream id matches.
        if (static_cast<int>(dmaInputBuffer.streamId) != stream->getStreamId()) {
            ALOGE("%s: Got an input buffer for stream %d but the stream id should be %d.",
                    __FUNCTION__, dmaInputBuffer.streamId, stream->getStreamId());
            return;
        }

        mOutputRequestQueue.pop_front();
    }

    // Transfer the DMA buffer.
    status_t res = mMessengerToClient->transferDmaBuffer(dmaInputBuffer.dmaHandle,
            outputRequest.buffers[0]->getPlaneData(0), outputRequest.buffers[0]->getDataSize());
    if (res != 0) {
        ALOGE("%s: Transferring DMA buffer failed: %s (%d).", __FUNCTION__,
                strerror(-res), res);

        // Put the output request back to the queue.
        std::unique_lock<std::mutex> lock(mQueueLock);
        mOutputRequestQueue.push_front(outputRequest);
        return;
    }

    handleCompletedCaptureForRequest(outputRequest, mockingEaselTimestampNs);
}

void SourceCaptureBlock::handleCompletedCaptureForRequest(const OutputRequest &outputRequest,
        int64_t easelTimestamp) {
    OutputResult result = {};
    result.buffers = outputRequest.buffers;
    result.route = outputRequest.route;
    result.metadata.frameMetadata = std::make_shared<FrameMetadata>();
    result.metadata.frameMetadata->easelTimestamp = easelTimestamp;

    // Put the output result to pending queue waiting for the frame metadata to arrive.
    {
        std::unique_lock<std::mutex> lock(mPendingOutputResultQueueLock);
        mPendingOutputResultQueue.push_back(result);
    }

    // Notify the client of the Easel timestamp.
    mMessengerToClient->notifyFrameEaselTimestampAsync(easelTimestamp);
}

void SourceCaptureBlock::sendOutputResult(const OutputResult &result) {
    auto pipeline = mPipeline.lock();
    if (pipeline == nullptr) {
        ALOGE("%s: Pipeline is destroyed.", __FUNCTION__);
        return;
    }

    pipeline->outputDone(result);
}

void SourceCaptureBlock::abortOutputRequest(const OutputRequest &request) {
    auto pipeline = mPipeline.lock();
    if (pipeline == nullptr) {
        ALOGE("%s: Pipeline is destroyed.", __FUNCTION__);
        return;
    }

    pipeline->outputRequestAbort(request);
}

void SourceCaptureBlock::notifyFrameMetadata(const FrameMetadata &metadata) {
    ALOGV("%s: got frame metadata for timestamp %" PRId64, __FUNCTION__, metadata.easelTimestamp);

    std::unique_lock<std::mutex> lock(mPendingOutputResultQueueLock);
    // Look for a pending output result that has the same timestamp.
    auto result = mPendingOutputResultQueue.begin();
    while (result != mPendingOutputResultQueue.end()) {
        if (result->metadata.frameMetadata->easelTimestamp == metadata.easelTimestamp) {
            *result->metadata.frameMetadata = metadata;

            // Send out the completed output result.
            sendOutputResult(*result);
            mPendingOutputResultQueue.erase(result);
            return;
        }

        result++;
    }

    ALOGE("%s: Cannot find an output buffer with easel timestamp %" PRId64, __FUNCTION__,
            metadata.easelTimestamp);
}

DequeueRequestThread::DequeueRequestThread(
        SourceCaptureBlock* parent) : mParent(parent), mExiting(false) {
    mDequeueRequestThread = std::make_unique<std::thread>(dequeueRequestThread, parent);
}

DequeueRequestThread::~DequeueRequestThread() {
    signalExit();
    mDequeueRequestThread->join();
    mDequeueRequestThread = nullptr;
}

void DequeueRequestThread::addPendingRequest(PipelineBlock::OutputRequest request) {
    std::unique_lock<std::mutex> lock(mDequeueThreadLock);
    mPendingCaptureRequests.push_back(request);
    mEventCondition.notify_one();
}

void DequeueRequestThread::signalExit() {
    {
        std::unique_lock<std::mutex> lock(mDequeueThreadLock);
        mExiting = true;
        mEventCondition.notify_one();
    }
}

void DequeueRequestThread::dequeueRequestThreadLoop() {
    while (1) {
        // Wait for new event for pending request.
        bool waitForRequest = false;
        {
            std::unique_lock<std::mutex> lock(mDequeueThreadLock);
            if (mPendingCaptureRequests.size() == 0 && !mExiting) {
                mEventCondition.wait(lock,
                        [&] { return mPendingCaptureRequests.size() > 0 || mExiting; });
            }

            if (mPendingCaptureRequests.size() > 0) {
                waitForRequest = true;
            }
        }

        // We have to wait for requests that have been sent to capture service.
        // TODO: A way to abort requests so we can exit early. b/35676087
        if (waitForRequest) {
            ALOGV("%s: Waiting for a completed request from capture service.", __FUNCTION__);
            CaptureFrameBuffer *frameBuffer = mParent->mCaptureService->DequeueCompletedRequest();
            if (frameBuffer == nullptr) {
                ALOGE("%s: DequeueCompletedRequest return NULL. Trying again.", __FUNCTION__);
                continue;
            }

            ALOGV("%s: Dequeued a completed request from capture service.", __FUNCTION__);

            PipelineBlock::OutputRequest request = {};
            bool foundRequest = false;

            // Find the pending request
            {
                std::unique_lock<std::mutex> lock(mDequeueThreadLock);
                for (auto pendingRequest = mPendingCaptureRequests.begin();
                        pendingRequest != mPendingCaptureRequests.end(); pendingRequest++) {
                    PipelineCaptureFrameBuffer* pipelineBuffer =
                            static_cast<PipelineCaptureFrameBuffer*>(pendingRequest->buffers[0]);
                    if (pipelineBuffer == nullptr ||
                        pipelineBuffer->getCaptureFrameBuffer() != frameBuffer) {
                        continue;
                    }

                    // Found the output request.
                    request = *pendingRequest;
                    mPendingCaptureRequests.erase(pendingRequest);
                    foundRequest = true;
                    break;
                }
            }

            if (!foundRequest) {
                ALOGE("%s: Cannot find a pending request for this frame buffer.", __FUNCTION__);
                continue;
            }

            int64_t syncedEaselTimeNs = 0;
            EaselControlServer::localToApSynchronizedClockBoottime(
                    frameBuffer->GetTimestampStartNs(), &syncedEaselTimeNs);

            mParent->handleCompletedCaptureForRequest(request, syncedEaselTimeNs);
        } else if (mExiting) {
            return;
        }
    }
}

} // pbcamera
