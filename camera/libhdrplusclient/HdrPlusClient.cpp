/*
 * Copyright 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
//#define LOG_NDEBUG 0
#define LOG_TAG "HdrPlusClient"
#include <utils/Log.h>

#define ENABLE_HDRPLUS_PROFILER 1

#include <camera/CameraMetadata.h>
#include <inttypes.h>
#include <stdlib.h>

#include "HdrPlusClient.h"

namespace android {

HdrPlusClient::HdrPlusClient() : mClientListener(nullptr), mServiceFatalErrorState(false) {
    mNotifyFrameMetadataThread = new NotifyFrameMetadataThread(&mMessengerToService);
    if (mNotifyFrameMetadataThread != nullptr) {
        mNotifyFrameMetadataThread->run("NotifyFrameMetadataThread");
    }
}

HdrPlusClient::~HdrPlusClient() {
    disconnect();
    if (mNotifyFrameMetadataThread != nullptr) {
        mNotifyFrameMetadataThread->requestExit();
        mNotifyFrameMetadataThread->join();
    }
}

status_t HdrPlusClient::connect(HdrPlusClientListener *listener) {
    ALOGV("%s", __FUNCTION__);

    if (mServiceFatalErrorState) {
        ALOGE("%s: HDR+ service is in a fatal error state.", __FUNCTION__);
        return NO_INIT;
    }

    // Connect to the messenger for sending messages to HDR+ service.
    status_t res = mMessengerToService.connect(*this);
    if (res != OK) {
        ALOGE("%s: Connecting service messenger failed: %s (%d)", __FUNCTION__, strerror(-res),
                res);
        disconnect();
        return res;
    }

    Mutex::Autolock l(mClientListenerLock);
    mClientListener = listener;

    return OK;
}

void HdrPlusClient:: failAllPendingRequestsLocked() {
    // Return all pending requests as failed results.
    Mutex::Autolock requestLock(mPendingRequestsLock);
    for (auto & pendingRequest : mPendingRequests) {
        pbcamera::CaptureResult result = {};
        result.requestId = pendingRequest.request.id;
        result.outputBuffers = pendingRequest.request.outputBuffers;
        mClientListener->onFailedCaptureResult(&result);
    }
    mPendingRequests.clear();

    return;
}

void HdrPlusClient::disconnect() {
    ALOGV("%s", __FUNCTION__);

    // Return all pending results and clear the listener to make sure no more callbacks will be
    // invoked.
    {
        Mutex::Autolock listenerLock(mClientListenerLock);
        if (mClientListener != nullptr) {
            failAllPendingRequestsLocked();
            mClientListener = nullptr;
        }
        mApEaselMetadataManager.clear();
    }

    // Disconnect from the service.
    mMessengerToService.disconnect();
}

status_t HdrPlusClient::setStaticMetadata(const camera_metadata_t &staticMetadata) {
    if (mServiceFatalErrorState) {
        ALOGE("%s: HDR+ service is in a fatal error state.", __FUNCTION__);
        return NO_INIT;
    }

    std::shared_ptr<CameraMetadata> staticMetadataSrc = std::make_shared<CameraMetadata>();
    *staticMetadataSrc.get() = &staticMetadata;

    std::shared_ptr<pbcamera::StaticMetadata> staticMetadataDest;
    status_t res = ApEaselMetadataManager::convertAndReturnStaticMetadata(&staticMetadataDest,
            staticMetadataSrc);
    if (res != 0) {
        ALOGE("%s: Converting static metadata failed: %s (%d).", __FUNCTION__, strerror(-res), res);
        return res;
    }

    {
        // This is to workaround HAL that doesn't support dynamic black level. Save static black
        // level to use as dynamic black level later.
        camera_metadata_entry entry = staticMetadataSrc->find(ANDROID_SENSOR_BLACK_LEVEL_PATTERN);
        if (entry.count == 4) {
            for (size_t i = 0; i < entry.count; i++) {
                mBlackLevelPattern[i] = entry.data.i32[i];
            }
        }
    }

    return mMessengerToService.setStaticMetadata(*staticMetadataDest.get());
}

status_t HdrPlusClient::configureStreams(const pbcamera::InputConfiguration &inputConfig,
            const std::vector<pbcamera::StreamConfiguration> &outputConfigs) {
    ALOGV("%s", __FUNCTION__);

    if (mServiceFatalErrorState) {
        ALOGE("%s: HDR+ service is in a fatal error state.", __FUNCTION__);
        return NO_INIT;
    }

    return mMessengerToService.configureStreams(inputConfig, outputConfigs);
}

status_t HdrPlusClient::setZslHdrPlusMode(bool enabled) {
    ALOGV("%s", __FUNCTION__);

    if (mServiceFatalErrorState) {
        ALOGE("%s: HDR+ service is in a fatal error state.", __FUNCTION__);
        return NO_INIT;
    }

    return mMessengerToService.setZslHdrPlusMode(enabled);
}

status_t HdrPlusClient::submitCaptureRequest(pbcamera::CaptureRequest *request) {
    ALOGV("%s", __FUNCTION__);

    if (mServiceFatalErrorState) {
        ALOGE("%s: HDR+ service is in a fatal error state.", __FUNCTION__);
        return NO_INIT;
    }

    // Lock here to prevent the case where the result comes back very quickly and couldn't
    // find the request in mPendingRequests.
    Mutex::Autolock l(mPendingRequestsLock);

    PendingRequest pendingRequest;
    pendingRequest.request = *request;

    START_PROFILER_TIMER(pendingRequest.timer);

    // Send the request to HDR+ service.
    status_t res = mMessengerToService.submitCaptureRequest(request);
    if (res != 0) {
        ALOGE("%s: Sending capture request to service failed: %s (%d).", __FUNCTION__,
                strerror(-res), res);
        return res;
    }

    // Push the request to pending request queue to look up when HDR+ service returns the result.
    mPendingRequests.push_back(std::move(pendingRequest));

    return 0;
}

void HdrPlusClient::notifyInputBuffer(const pbcamera::StreamBuffer &inputBuffer,
        int64_t timestampNs) {
    ALOGV("%s", __FUNCTION__);

    if (mServiceFatalErrorState) {
        ALOGE("%s: HDR+ service is in a fatal error state.", __FUNCTION__);
        return;
    }

    mMessengerToService.notifyInputBuffer(inputBuffer, timestampNs);
}

void HdrPlusClient::notifyFrameMetadata(uint32_t frameNumber,
        const camera_metadata_t &resultMetadata, bool lastMetadata) {
    ALOGV("%s", __FUNCTION__);

    if (mServiceFatalErrorState) {
        ALOGE("%s: HDR+ service is in a fatal error state.", __FUNCTION__);
        return;
    }

    std::shared_ptr<pbcamera::FrameMetadata> frameMetadata;
    std::shared_ptr<CameraMetadata> cameraMetadata;

    {
        Mutex::Autolock clientLock(mFrameNumPartialMetadataMapLock);
        auto collectedMetadataIter = mFrameNumPartialMetadataMap.find(frameNumber);

        // If this is not the last metadata. Save it to the partial metadata map and return.
        if (!lastMetadata) {
            if (collectedMetadataIter != mFrameNumPartialMetadataMap.end()) {
                collectedMetadataIter->second->append(&resultMetadata);
            } else {
                cameraMetadata = std::make_shared<CameraMetadata>();
                *cameraMetadata.get() = &resultMetadata;
                mFrameNumPartialMetadataMap.emplace(frameNumber, cameraMetadata);
            }
            return;
        }

        // This is the last metadata. If there are partial metadata received previously,
        // combine the metadata.
        if (collectedMetadataIter != mFrameNumPartialMetadataMap.end()) {
            cameraMetadata = collectedMetadataIter->second;
            cameraMetadata->append(&resultMetadata);
            mFrameNumPartialMetadataMap.erase(collectedMetadataIter);
        } else {
            cameraMetadata = std::make_shared<CameraMetadata>();
            *cameraMetadata.get() = &resultMetadata;
        }
    }

    {
        // This is to workaround HAL that doesn't support dynamic black level. Use static black
        // level as dynamic black level later.
        camera_metadata_entry entry = cameraMetadata->find(ANDROID_SENSOR_DYNAMIC_BLACK_LEVEL);
        if (entry.count != 4) {
            cameraMetadata->update(ANDROID_SENSOR_DYNAMIC_BLACK_LEVEL, &mBlackLevelPattern[0],
                    mBlackLevelPattern.size());
        }
    }

    IF_ALOGV() {
        // Log AP timestamp and exposure time.

        int64_t timestamp = 0, exposureTime = 0;
        camera_metadata_entry entry = cameraMetadata->find(ANDROID_SENSOR_TIMESTAMP);
        if (entry.count != 0) {
            timestamp = entry.data.i64[0];
        }

        entry = cameraMetadata->find(ANDROID_SENSOR_EXPOSURE_TIME);
        if (entry.count != 0) {
            exposureTime = entry.data.i64[0];
        }

        ALOGV("%s: Got an AP timestamp: %" PRId64" exposureTime %" PRId64" ns", __FUNCTION__,
                timestamp, exposureTime);
    }

    // Add the AP's camera metadata to metadata manager. If a PB frame metadata is ready, send
    // it to the HDR+ service.
    mApEaselMetadataManager.addCameraMetadata(cameraMetadata, &frameMetadata);
    if (frameMetadata != nullptr) {
        if (mNotifyFrameMetadataThread == nullptr) {
            ALOGE("%s: Notify frame metadata thread is not initialized.", __FUNCTION__);
            return;
        }

        mNotifyFrameMetadataThread->queueFrameMetadata(frameMetadata);
    }
}

// Callbacks from HDR+ service.
void HdrPlusClient::notifyFrameEaselTimestamp(int64_t easelTimestampNs) {
    ALOGV("%s: Got an easel timestamp %" PRId64, __FUNCTION__, easelTimestampNs);

    std::shared_ptr<pbcamera::FrameMetadata> frameMetadata;

    // Add the Easel timestamp to metadata manager. If a PB frame metadata is ready, send it
    // to the HDR+ service.
    mApEaselMetadataManager.addEaselTimestamp(easelTimestampNs, &frameMetadata);
    if (frameMetadata != nullptr) {
        if (mNotifyFrameMetadataThread == nullptr) {
            ALOGE("%s: Notify frame metadata thread is not initialized.", __FUNCTION__);
            return;
        }

        mNotifyFrameMetadataThread->queueFrameMetadata(frameMetadata);
    }
}

void HdrPlusClient::notifyServiceClosed() {
    // Return all pending requests.
    Mutex::Autolock listenerLock(mClientListenerLock);

    if (mClientListener != nullptr) {
        // If client listener is still valid, service is not closed by client.
        mServiceFatalErrorState = true;
        failAllPendingRequestsLocked();
        mClientListener->onFatalError();
    }
}

void HdrPlusClient::notifyDmaCaptureResult(pbcamera::DmaCaptureResult *result) {
    if (result == nullptr) return;

    if (result->buffer.dmaHandle == nullptr) {
        ALOGE("%s: Received a DMA buffer but DMA handle is null.", __FUNCTION__);
        return;
    }

    ALOGV("%s: Received a buffer: request %d stream %d DMA data size %d", __FUNCTION__,
            result->requestId, result->buffer.streamId, result->buffer.dmaDataSize);

    bool successfulResult = true;

    Mutex::Autolock clientLock(mClientListenerLock);
    Mutex::Autolock requestLock(mPendingRequestsLock);

    // Find the output buffer from the pending requests.
    for (uint32_t i = 0; i < mPendingRequests.size(); i++) {
        if (mPendingRequests[i].request.id == result->requestId) {
            for (uint32_t j = 0; j < mPendingRequests[i].request.outputBuffers.size(); j++) {
                pbcamera::StreamBuffer *requestBuffer =
                        &mPendingRequests[i].request.outputBuffers[j];
                if (requestBuffer->streamId == result->buffer.streamId) {
                    // Found the output buffer. Now transfer the content of DMA buffer to this
                    // output buffer.
                    status_t res = mMessengerToService.transferDmaBuffer(result->buffer.dmaHandle,
                            requestBuffer->dmaBufFd, requestBuffer->data, requestBuffer->dataSize);

                    if (res != 0) {
                        ALOGE("%s: Transferring DMA buffer failed: %s (%d).", __FUNCTION__,
                                strerror(-res), res);
                        successfulResult = false;
                    }

                    END_PROFILER_TIMER(mPendingRequests[i].timer);

                    // Get the result metadata using the AP timestamp.
                    const camera_metadata_t *resultMetadata = nullptr;
                    std::shared_ptr<CameraMetadata> cameraMetadata;
                    res = mApEaselMetadataManager.getCameraMetadata(&cameraMetadata,
                            result->metadata.timestamp);
                    if (res != OK) {
                        ALOGE("%s: Failed to get camera metadata for timestamp %" PRId64
                                ": %s (%d)", __FUNCTION__, result->metadata.timestamp,
                                strerror(-res), res);
                        successfulResult = false;
                    } else {
                        resultMetadata = cameraMetadata->getAndLock();
                    }

                    pbcamera::CaptureResult clientResult = {};
                    clientResult.requestId = result->requestId;
                    clientResult.outputBuffers.push_back(*requestBuffer);

                    if (successfulResult) {
                        // Invoke client listener callback for the capture result.
                        mClientListener->onCaptureResult(&clientResult, *resultMetadata);
                    } else {
                        // Invoke client listener callback for the failed capture result.
                        mClientListener->onFailedCaptureResult(&clientResult);
                    }

                    // Release metadata
                    if (resultMetadata != nullptr) {
                        cameraMetadata->unlock(resultMetadata);
                    }

                    // Remove the buffer from pending request.
                    auto bufferIter = mPendingRequests[i].request.outputBuffers.begin() + j;
                    mPendingRequests[i].request.outputBuffers.erase(bufferIter);

                    // Remove the pending request if it has no more pending buffers.
                    if (mPendingRequests[i].request.outputBuffers.size() == 0) {
                        auto requestIter = mPendingRequests.begin() + i;
                        mPendingRequests.erase(requestIter);
                    }

                    return;
                }
            }
        }
    }

    ALOGE("%s: Could not find a buffer for this result: request %d stream %d.", __FUNCTION__,
            result->requestId, result->buffer.streamId);
}

NotifyFrameMetadataThread::NotifyFrameMetadataThread(
        pbcamera::MessengerToHdrPlusService* messenger) : mMessenger(messenger),
        mExitRequested(false) {
}

NotifyFrameMetadataThread::~NotifyFrameMetadataThread() {
    requestExit();
}

void NotifyFrameMetadataThread::requestExit() {
    std::unique_lock<std::mutex> lock(mEventLock);
    mExitRequested = true;
    mEventCond.notify_one();
}

void NotifyFrameMetadataThread::queueFrameMetadata(
        std::shared_ptr<pbcamera::FrameMetadata> frameMetadata) {
    std::unique_lock<std::mutex> lock(mEventLock);

    mFrameMetadataQueue.push(frameMetadata);
    mEventCond.notify_one();
}

bool NotifyFrameMetadataThread::threadLoop() {
    if (mMessenger == nullptr) {
        ALOGE("%s: mMessenger is nullptr. Exit.", __FUNCTION__);
        return false;
    }

    std::shared_ptr<pbcamera::FrameMetadata> frameMetadata;

    // Wait for next frame metadata or exit request.
    {
        std::unique_lock<std::mutex> lock(mEventLock);
        if (mFrameMetadataQueue.size() == 0 && !mExitRequested) {
            mEventCond.wait(lock,
                    [&] { return mFrameMetadataQueue.size() > 0 || mExitRequested; });
        }

        if (mExitRequested) {
            ALOGV("%s: thread exiting.", __FUNCTION__);
            return false;
        }

        frameMetadata = mFrameMetadataQueue.front();
        mFrameMetadataQueue.pop();
    }

    mMessenger->notifyFrameMetadataAsync(*frameMetadata.get());

    return true;
}

} // namespace android
