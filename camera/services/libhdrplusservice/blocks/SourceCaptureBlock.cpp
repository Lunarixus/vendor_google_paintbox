//#define LOG_NDEBUG 0
#define LOG_TAG "SourceCaptureBlock"
#include <log/log.h>

#include <inttypes.h>
#include <system/graphics.h>

#include <hardware/gchips/paintbox/system/include/dram_controller_settings.h>

#include "CaptureServiceConsts.h"
#include "SourceCaptureBlock.h"
#include "HdrPlusPipeline.h"

namespace pbcamera {

static void dequeueRequestThread(DequeueRequestThread* thread) {
    if (thread != nullptr) {
        thread->dequeueRequestThreadLoop();
    }
}

SourceCaptureBlock::SourceCaptureBlock(std::shared_ptr<MessengerToHdrPlusClient> messenger,
        const paintbox::CaptureConfig &config = {}) :
        PipelineBlock("SourceCaptureBlock", BLOCK_EVENT_TIMEOUT_MS),
        mMessengerToClient(messenger),
        mCaptureConfig(config),
        mCaptureServicePaused(false),
        mClockMode(EaselControlServer::ClockMode::Max),
        mLastRequestedFrameCounterId(kInvalidFrameCounterId),
        mLastFinishedFrameCounterId(kInvalidFrameCounterId) {
    // Check if capture config is valid.
    if (mCaptureConfig.stream_config_list.size() > 0) {
        mIsMipiInput = true;
    } else {
        mIsMipiInput = false;
    }
}

SourceCaptureBlock::~SourceCaptureBlock() {
    // Enforce the right order to destroy capture service.
    std::unique_lock<std::mutex> lock(mSourceCaptureLock);
    destroyCaptureServiceLocked();
}

status_t SourceCaptureBlock::createCaptureServiceLocked() {
    if (mCaptureService != nullptr) {
        return -EEXIST;
    }

    ALOGI("%s: Creating new catpure service", __FUNCTION__);
    mCaptureService = paintbox::CaptureService::CreateInstance(mCaptureConfig);
    if (mCaptureService == nullptr) {
        ALOGE("%s: Initializing capture service failed.", __FUNCTION__);
        return -ENODEV;
    }

    int32_t frameCounterId = kInvalidFrameCounterId;
    {
        std::unique_lock<std::mutex> lock(mFrameCounterLock);
        frameCounterId = ++mLastRequestedFrameCounterId;
    }

    // Create a dequeue request thread.
    mDequeueRequestThread = std::make_unique<DequeueRequestThread>(this);
    mDequeueRequestThread->requestFrameCounterNotification(kStableFrameCount, frameCounterId);
    return 0;
}

void SourceCaptureBlock::destroyCaptureServiceLocked() {
    mDequeueRequestThread = nullptr;
    mCaptureService = nullptr;
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

    std::shared_ptr<SourceCaptureBlock> block;
    if (sensorMode != nullptr) {
        // Create a source capture block with capture service for capturing from Easel MIPI.
        paintbox::MipiDataTypeCsi2 dataType;
        uint32_t bitsPerPixel = 0;
        switch (sensorMode->format) {
            case HAL_PIXEL_FORMAT_RAW10:
                dataType = paintbox::RAW10;
                bitsPerPixel = 10;
                break;
            default:
                ALOGE("%s: Only HAL_PIXEL_FORMAT_RAW10 is supported but sensor mode has %d",
                        __FUNCTION__, sensorMode->format);
                return nullptr;
        }

        paintbox::MipiRxPort mipiRxPort;
        switch (sensorMode->cameraId) {
            case 0:
                mipiRxPort = paintbox::MipiRxPort::RX0;
                break;
            case 1:
                mipiRxPort = paintbox::MipiRxPort::RX1;
                break;
            default:
                ALOGE("%s: Camera ID (%u) is not supported.", __FUNCTION__, sensorMode->cameraId);
                return nullptr;
        }

        // Create a capture service.
        std::vector<paintbox::CaptureStreamConfig> captureStreamConfigs;
        captureStreamConfigs.push_back({ dataType, sensorMode->pixelArrayWidth,
                sensorMode->pixelArrayHeight, bitsPerPixel,
                capture_service_consts::kBusAlignedStreamConfig });
        paintbox::CaptureConfig config = { mipiRxPort,
                capture_service_consts::kMainImageVirtualChannelId,
                capture_service_consts::kCaptureFrameBufferFactoryTimeoutMs,
                captureStreamConfigs };

        block = std::shared_ptr<SourceCaptureBlock>(new SourceCaptureBlock(messenger, config));

    } else {
        // Create a source capture block to receive input buffers from AP.
        block = std::shared_ptr<SourceCaptureBlock>(new SourceCaptureBlock(messenger));
    }

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

namespace {

bool isGoodThermalCondition(EaselControlServer::ThermalCondition condition) {
    // Empirically determined that good thermal condition is less than Medium
    return (condition < EaselControlServer::ThermalCondition::Medium);
}

}  // namespace

void SourceCaptureBlock::notifyIpuProcessingStart(bool continuousCapturing) {
    // For input buffers coming from the client via notifyDmaInputBuffer(), there is nothing to do
    // here.
    if (!mIsMipiInput) return;

    std::unique_lock<std::mutex> lock(mSourceCaptureLock);

    {
        // Increment last requested frame counter ID so older frame counter ID will be ignored.
        std::unique_lock<std::mutex> lock(mFrameCounterLock);
        mLastRequestedFrameCounterId++;
    }

    bool goodThermal = isGoodThermalCondition(EaselControlServer::getThermalCondition());
    // Pause if continuous capturing is false or we need to change clock mode, or if we have a
    // potential thermal situation.
    bool doPause = !continuousCapturing || !goodThermal ||
        (mClockMode != EaselControlServer::ClockMode::Functional) ||
        EaselControlServer::isNewThermalCondition();
    if (doPause) {
        pauseCaptureServiceLocked();
        ALOGD("%s: Continuous Capture paused\n", __FUNCTION__);

        EaselControlServer::ThermalCondition newThermal =
            EaselControlServer::setClockMode(EaselControlServer::ClockMode::Functional);
        goodThermal = isGoodThermalCondition(newThermal);
        mClockMode = EaselControlServer::ClockMode::Functional;
    }

    // Do continuous capture only if we have good thermal condition.
    if (continuousCapturing && goodThermal) {
        ALOGD("%s: Do Continuous Capture\n", __FUNCTION__);
        paintbox::dram_controller::ContinuousCaptureSettings();
        if (doPause) {
            resumeCaptureServiceLocked(false, kInvalidFrameCounterId);
            ALOGD("%s: Capture Service resumed\n", __FUNCTION__);
        }
    } else {
        ALOGD("%s: Not Continuous Capture\n", __FUNCTION__);
        if (continuousCapturing) {
            ALOGD("%s: Did not resume Continuous Capture due to bad thermal\n", __FUNCTION__);
        }
        paintbox::dram_controller::DefaultSettings();
    }
}

void SourceCaptureBlock::notifyIpuProcessingDone() {
    // For input buffers coming from the client via notifyDmaInputBuffer(), there is nothing to do
    // here.
    if (!mIsMipiInput) return;

    int32_t frameCounterId = kInvalidFrameCounterId;
    {
        std::unique_lock<std::mutex> lock(mFrameCounterLock);
        frameCounterId = mLastRequestedFrameCounterId;
    }

    std::unique_lock<std::mutex> lock(mSourceCaptureLock);
    resumeCaptureServiceLocked(true, frameCounterId);
}

void SourceCaptureBlock::notifyFrameCounterDone(int32_t frameCounterId) {
    std::unique_lock<std::mutex> lock(mFrameCounterLock);
    mLastFinishedFrameCounterId = frameCounterId;
}

void SourceCaptureBlock::changeToCaptureClockModeLocked() {
    pauseCaptureServiceLocked();
    EaselControlServer::setClockMode(EaselControlServer::ClockMode::Capture);
    ALOGV("%s: Clock mode is Capture", __FUNCTION__);
    mClockMode = EaselControlServer::ClockMode::Capture;
    resumeCaptureServiceLocked(false, kInvalidFrameCounterId);
}

void SourceCaptureBlock::pauseCaptureServiceLocked() {
    if (mCaptureServicePaused) return;

    mDequeueRequestThread->pause();
    mCaptureServicePaused = true;
}

void SourceCaptureBlock::resumeCaptureServiceLocked(bool startFrameCounter, int32_t frameCountId) {
    if (startFrameCounter) {
         mDequeueRequestThread->requestFrameCounterNotification(kStableFrameCount, frameCountId);
    }

    if (!mCaptureServicePaused) {
        return;
    }

    mDequeueRequestThread->resume();
    mCaptureServicePaused = false;
    notifyWorkerThreadEvent();
}

bool SourceCaptureBlock::doWorkLocked() {
    ALOGV("%s", __FUNCTION__);

    // Create a timestamp notification thread to send Easel timestamps if it doesn't exist yet.
    if (mTimestampNotificationThread == nullptr) {
        mTimestampNotificationThread =
            std::make_unique<TimestampNotificationThread>(mMessengerToClient);
    }

    // For input buffers coming from the client via notifyDmaInputBuffer(), there is nothing to do
    // here.
    if (!mIsMipiInput) return false;

    std::unique_lock<std::mutex> lock(mSourceCaptureLock);

    if (mCaptureService == nullptr) {
        status_t res = createCaptureServiceLocked();
        if (res != 0) {
            ALOGE("%s: Creating capture service failed: %s (%d)", __FUNCTION__, strerror(-res),
                    res);
            return false;
        }
    }



    {
        std::unique_lock<std::mutex> lock(mFrameCounterLock);
        // If last requested frame counter ID is valid and it's the same as the last frame counter
        // ID received from dequeue request thread, capture service is in a stable state to change
        // clock mode.
        if (mLastRequestedFrameCounterId != kInvalidFrameCounterId &&
                mLastRequestedFrameCounterId == mLastFinishedFrameCounterId) {
            changeToCaptureClockModeLocked();
            mLastFinishedFrameCounterId = kInvalidFrameCounterId;
        }
    }

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

status_t SourceCaptureBlock::flushLocked() {

    // Capture service does not support flush so we need to destroy the dequeue request thread
    // and destroy camera service to flush capture service. Capture service will be created again
    // when handling a request.
    // b/35676087.
    {
        std::unique_lock<std::mutex> lock(mSourceCaptureLock);
        destroyCaptureServiceLocked();
    }

    // Return incomplete output results.
    {
        std::unique_lock<std::mutex> lock(mPendingOutputResultQueueLock);
        for (auto result : mPendingOutputResultQueue) {
            abortOutputRequest(result);
        }

        mPendingOutputResultQueue.clear();
    }

    return 0;
}

void SourceCaptureBlock::removeTimedoutPendingOutputResult() {
    int64_t now;
    status_t res = EaselControlServer::getApSynchronizedClockBoottime(&now);
    if (res != 0) {
        ALOGE("%s: Getting AP synchronized clock boot time failed.", __FUNCTION__);
        return;
    }

    std::unique_lock<std::mutex> lock(mPendingOutputResultQueueLock);

    ALOGI("%s: There are %lu pending output results", __FUNCTION__,
            mPendingOutputResultQueue.size());

    auto result = mPendingOutputResultQueue.begin();
    while (result != mPendingOutputResultQueue.end()) {
        // Sanity check the timestamp.
        int64_t frameTimestamp = result->metadata.frameMetadata->easelTimestamp;
        if (now - frameTimestamp > FRAME_METADATA_TIMEOUT_NS) {
            // If the pending result has not received frame metadata from AP after a timeout
            // duration, abort the output request.
            ALOGW("%s: AP may have dropped a frame. Easel timestamp %" PRId64 " now is %" PRId64,
                    __FUNCTION__, result->metadata.frameMetadata->easelTimestamp, now);
            abortOutputRequest(*result);
            result = mPendingOutputResultQueue.erase(result);
        } else if (frameTimestamp > now) {
            // Easel timestamp is wrong. Abort this request.
            ALOGE("%s: Easel timestamp is wrong: %" PRId64 " now is %" PRId64, __FUNCTION__,
                    result->metadata.frameMetadata->easelTimestamp, now);
            abortOutputRequest(*result);
            result = mPendingOutputResultQueue.erase(result);
        } else {
            ALOGV("%s: this result timestamp %" PRId64 " now %" PRId64, __FUNCTION__,
                result->metadata.frameMetadata->easelTimestamp, now);

            result++;
        }
    }
}

void SourceCaptureBlock::handleTimeoutLocked() {
    // Timeout is expected if it's paused.
    if (mCaptureServicePaused) return;

    ALOGI("%s: Source capture block timed out", __FUNCTION__);

    // Remove pending output results that have been around for a while if capturing from MIPI.
    if (mIsMipiInput) {
        removeTimedoutPendingOutputResult();
    }
}

status_t SourceCaptureBlock::transferDmaBuffer(const DmaImageBuffer &dmaInputBuffer,
        PipelineBuffer *buffer) {
    if (buffer == nullptr) return -EINVAL;

    // Allocate a temporary buffer for DMA transfer. This is to workaround b/62633675.
    // TODO: Remove this temporary buffer once we can get the fd for the ION buffer.
    auto temp = std::unique_ptr<void, void (*)(void*)>(malloc(buffer->getDataSize()), free);
    if (temp == nullptr) {
        ALOGE("%s: Can't allocate a temporary buffer for DMA transfer.", __FUNCTION__);
        return -ENOMEM;
    }

    // DMA transfer to the temporary buffer.
    status_t res = mMessengerToClient->transferDmaBuffer(dmaInputBuffer.dmaHandle, /*ionFd*/-1,
            temp.get(), buffer->getDataSize());
    if (res != 0) {
        ALOGE("%s: transfering DMA buffer failed: %s (%d)", __FUNCTION__, strerror(-res), res);
        return res;
    }

    res = buffer->lockData();
    if (res != 0) {
        ALOGE("%s: locking buffer data failed: %s (%d)", __FUNCTION__, strerror(-res), res);
        return res;
    }

    // Copy to the actual buffer.
    memcpy(buffer->getPlaneData(0), temp.get(), buffer->getDataSize());
    buffer->unlockData();

    return 0;
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

    status_t res = transferDmaBuffer(dmaInputBuffer, outputRequest.buffers[0]);
    if (res != 0) {
        ALOGE("%s: transferDmaBuffer failed: %s (%d)", __FUNCTION__, strerror(-res), res);

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
    if (mTimestampNotificationThread != nullptr) {
        mTimestampNotificationThread->notifyNewEaselTimestampNs(easelTimestamp);
    }
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
        } else if (result->metadata.frameMetadata->easelTimestamp < metadata.easelTimestamp) {
            ALOGE("%s: AP may have dropped a frame with Easel timestamp %" PRId64, __FUNCTION__,
                    result->metadata.frameMetadata->easelTimestamp);
            // AP may have dropped a frame. Abort this request.
            abortOutputRequest(*result);
            result = mPendingOutputResultQueue.erase(result);
        } else {
            result++;
        }
    }

    ALOGE("%s: Cannot find an output buffer with easel timestamp %" PRId64, __FUNCTION__,
            metadata.easelTimestamp);
}

void SourceCaptureBlock::requestCaptureToPreventFrameDrop() {
    // Capture service needs one more buffer to prevent a frame drop.
    // Take the oldest one from pending output result queue and abort it.
    std::unique_lock<std::mutex> resultLock(mPendingOutputResultQueueLock);
    if (mPendingOutputResultQueue.size() > 0) {
        abortOutputRequest(mPendingOutputResultQueue[0]);
        mPendingOutputResultQueue.pop_front();
    }
}

DequeueRequestThread::DequeueRequestThread(
        SourceCaptureBlock* parent) : mParent(parent), mFirstCaptureDone(false),
        mFrameCounter(0), mFrameCounterId(SourceCaptureBlock::kInvalidFrameCounterId),
        mState(STATE_RUNNING) {
    mThread = std::make_unique<std::thread>(dequeueRequestThread, this);
}

DequeueRequestThread::~DequeueRequestThread() {
    signalExit();
    mThread->join();
    mThread = nullptr;

    // Return all pending requests.
    for (auto request : mPendingCaptureRequests) {
        mParent->abortOutputRequest(request);
    }
}

void DequeueRequestThread::addPendingRequest(PipelineBlock::OutputRequest request) {
    std::unique_lock<std::mutex> lock(mDequeueThreadLock);
    mPendingCaptureRequests.push_back(request);
    mEventCondition.notify_one();
}

void DequeueRequestThread::signalExit() {
    std::unique_lock<std::mutex> lock(mDequeueThreadLock);
    mState = STATE_EXITING;
    mEventCondition.notify_one();
}

void DequeueRequestThread::pause() {
    std::unique_lock<std::mutex> lock(mDequeueThreadLock);
    mState = STATE_PAUSING;
    mEventCondition.notify_one();
    mStateChangedCondition.wait(lock, [&] { return mState == STATE_PAUSED; });
}

void DequeueRequestThread::resume() {
    std::unique_lock<std::mutex> lock(mDequeueThreadLock);
    mState = STATE_RESUMING;
    mEventCondition.notify_one();
    mStateChangedCondition.wait(lock, [&] { return mState == STATE_RUNNING; });
}

void DequeueRequestThread::requestFrameCounterNotification(uint32_t frameCount, int32_t requestId) {
    std::unique_lock<std::mutex> lock(mDequeueThreadLock);
    mFrameCounter = frameCount;
    mFrameCounterId = requestId;
}

void DequeueRequestThread::checkNumberPendingRequests() {
    bool needMoreRequest = false;

    {
        // Check if capture service needs more requests.
        std::unique_lock<std::mutex> lock(mDequeueThreadLock);
        if (mPendingCaptureRequests.size() < kMinNumPendingRequests) {
            needMoreRequest = true;
        }
    }

    if (needMoreRequest) {
        mParent->requestCaptureToPreventFrameDrop();
    }
}

void DequeueRequestThread::dequeueRequestThreadLoop() {
    while (1) {
        // Wait for new event for pending request.
        bool waitForRequest = false;
        {
            std::unique_lock<std::mutex> lock(mDequeueThreadLock);
            if ((mPendingCaptureRequests.size() == 0 || mState == STATE_PAUSED) &&
                    mState != STATE_EXITING) {
                mEventCondition.wait(lock, [&] { return (mPendingCaptureRequests.size() > 0 &&
                        mState != STATE_PAUSED) || mState == STATE_EXITING; });
            }

            // Handle the transient state.
            if (mState != STATE_RUNNING) {
                switch (mState) {
                    case STATE_PAUSING:
                        // When pausing is requeseted, pause capture service, change the state
                        // to paused, and notify the parent it's paused.
                        mParent->mCaptureService->Pause();
                        mState = STATE_PAUSED;
                        mStateChangedCondition.notify_one();
                        break;
                    case STATE_RESUMING:
                        // When resuming is requeseted, resume capture service, change the state
                        // to running, and notify the parent it's running.
                        mParent->mCaptureService->Resume();
                        mState = STATE_RUNNING;
                        mStateChangedCondition.notify_one();
                        break;
                    case STATE_EXITING:
                        // Upon exiting, pending requests in camera service have not be flushed.
                        // b/35676087. So after exit, mCaptureService must be destroyed before
                        // releasing all pending buffers.
                        ALOGV("%s: Exit thread loop.", __FUNCTION__);
                        return;

                    default:
                        break;
                }

                // All transient state is handled, continue to wait for the new state, or new
                // buffers.
                continue;
            }

            if (mPendingCaptureRequests.size() > 0) {
                waitForRequest = true;
            }
        }

        if (waitForRequest) {
            ALOGV("%s: Waiting for a completed request from capture service.", __FUNCTION__);
            paintbox::CaptureFrameBuffer *frameBuffer =
                    mParent->mCaptureService->DequeueCompletedRequest();
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

            paintbox::CaptureError err = frameBuffer->GetError();
            if (err != paintbox::CaptureError::SUCCESS) {
                ALOGE("%s: Request encountered an error: %s (%d)", __FUNCTION__,
                        GetCaptureErrorDesc(err), err);
                // Abort the request.
                mParent->abortOutputRequest(request);
                continue;
            }

            if (!mFirstCaptureDone) {
                int64_t now;
                status_t res = EaselControlServer::getApSynchronizedClockBoottime(&now);
                if (res != 0) {
                    ALOGE("%s: Getting AP synchronized clock boot time failed.", __FUNCTION__);
                }

                ALOGI("[EASEL_STARTUP_LATENCY] %s: First RAW capture done at %" PRId64 " ms",
                        __FUNCTION__, now / kNsPerMs);
                mFirstCaptureDone = true;
            }

            if (mFrameCounter > 0) {
                if (--mFrameCounter == 0) {
                    // Notify the parent.
                    mParent->notifyFrameCounterDone(mFrameCounterId);
                }
            }

            int64_t syncedEaselTimeNs = 0;
            EaselControlServer::localToApSynchronizedClockBoottime(
                    frameBuffer->GetTimestampStartNs(), &syncedEaselTimeNs);

            mParent->handleCompletedCaptureForRequest(request, syncedEaselTimeNs);

            // Check if we have enough pending requests.
            checkNumberPendingRequests();
        }
    }
}

static void timestampNotificationThreadLoop(TimestampNotificationThread *notificationThread) {
    if (notificationThread != nullptr) {
        notificationThread->threadLoop();
    }
}

TimestampNotificationThread::TimestampNotificationThread(
        std::shared_ptr<MessengerToHdrPlusClient> messengerToClient) :
        mMessengerToClient(messengerToClient),
        mExiting(false) {
    mThread = std::make_unique<std::thread>(timestampNotificationThreadLoop, this);
}

TimestampNotificationThread::~TimestampNotificationThread() {
    signalExit();
    if (mThread != nullptr) {
        mThread->join();
        mThread = nullptr;
    }
}

void TimestampNotificationThread::signalExit() {
    std::unique_lock<std::mutex> lock(mEventLock);
    mExiting = true;
    mEventCondition.notify_one();
}

void TimestampNotificationThread::notifyNewEaselTimestampNs(int64_t easelTimestampNs) {
    std::unique_lock<std::mutex> lock(mEventLock);
    mEaselTimestamps.push_back(easelTimestampNs);
    mEventCondition.notify_one();
}

void TimestampNotificationThread::threadLoop() {
    int64_t easelTimestampNs = 0;

    while (1) {
        {
            std::unique_lock<std::mutex> lock(mEventLock);

            // Wait until a new timestamp arrives or it's exiting.
            if (mEaselTimestamps.size() == 0 && !mExiting) {
                mEventCondition.wait(lock,
                        [&] { return mEaselTimestamps.size() > 0 || mExiting; });
            }

            if (mExiting) {
                ALOGV("%s: Exiting.", __FUNCTION__);
                return;
            }

            easelTimestampNs = mEaselTimestamps[0];
            mEaselTimestamps.pop_front();
        }

        mMessengerToClient->notifyFrameEaselTimestampAsync(easelTimestampNs);
    }
}

} // pbcamera
