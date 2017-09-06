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
#define ATRACE_TAG ATRACE_TAG_CAMERA
#define LOG_TAG "HdrPlusClientImpl"
#include <log/log.h>

#define ENABLE_HDRPLUS_PROFILER 1

#include <utils/Trace.h>

#include <CameraMetadata.h>
#include <cutils/properties.h>
#include <dirent.h>
#include <fstream>
#include <inttypes.h>
#include <QCamera3VendorTags.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "HdrPlusClientImpl.h"

using ::android::hardware::camera::common::V1_0::helper::CameraMetadata;
namespace android {

HdrPlusClientImpl::HdrPlusClientImpl() : mClientListener(nullptr), mServiceFatalErrorState(false) {
    mNotifyFrameMetadataThread = new NotifyFrameMetadataThread(&mMessengerToService);
    if (mNotifyFrameMetadataThread != nullptr) {
        mNotifyFrameMetadataThread->run("NotifyFrameMetadataThread");
    }
}

HdrPlusClientImpl::~HdrPlusClientImpl() {
    disconnect();
    if (mNotifyFrameMetadataThread != nullptr) {
        mNotifyFrameMetadataThread->requestExit();
        mNotifyFrameMetadataThread->join();
    }
}

status_t HdrPlusClientImpl::connect(HdrPlusClientListener *listener) {
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

void HdrPlusClientImpl:: failAllPendingRequestsLocked() {
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

void HdrPlusClientImpl::disconnect() {
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

status_t HdrPlusClientImpl::setStaticMetadata(const camera_metadata_t &staticMetadata) {
    if (mServiceFatalErrorState) {
        ALOGE("%s: HDR+ service is in a fatal error state.", __FUNCTION__);
        return NO_INIT;
    }

    CameraMetadata staticMetadataSrc;
    staticMetadataSrc = &staticMetadata;

    pbcamera::StaticMetadata staticMetadataDest;
    status_t res = ApEaselMetadataManager::convertAndReturnStaticMetadata(&staticMetadataDest,
            staticMetadataSrc);
    if (res != 0) {
        ALOGE("%s: Converting static metadata failed: %s (%d).", __FUNCTION__, strerror(-res), res);
        return res;
    }

    {
        // This is to workaround HAL that doesn't support dynamic black level. Save static black
        // level to use as dynamic black level later.
        camera_metadata_entry entry = staticMetadataSrc.find(ANDROID_SENSOR_BLACK_LEVEL_PATTERN);
        if (entry.count == 4) {
            for (size_t i = 0; i < entry.count; i++) {
                mBlackLevelPattern[i] = entry.data.i32[i];
            }
        }
    }

    if (property_get_bool("persist.gcam.debug", false)) {
        staticMetadataDest.debugParams |= (pbcamera::DEBUG_PARAM_SAVE_GCAME_INPUT_METERING |
                                           pbcamera::DEBUG_PARAM_SAVE_GCAME_INPUT_PAYLOAD |
                                           pbcamera::DEBUG_PARAM_SAVE_GCAME_TEXT);
    }

    if (property_get_bool("persist.gcam.save_text", false)) {
        staticMetadataDest.debugParams |= pbcamera::DEBUG_PARAM_SAVE_GCAME_TEXT;
    }

    if (property_get_bool("persist.gcam.save_metering", false)) {
        staticMetadataDest.debugParams |= pbcamera::DEBUG_PARAM_SAVE_GCAME_INPUT_METERING;
    }

    if (property_get_bool("persist.gcam.save_payload", false)) {
        staticMetadataDest.debugParams |= pbcamera::DEBUG_PARAM_SAVE_GCAME_INPUT_PAYLOAD;
    }

    return mMessengerToService.setStaticMetadata(staticMetadataDest);
}

status_t HdrPlusClientImpl::configureStreams(const pbcamera::InputConfiguration &inputConfig,
            const std::vector<pbcamera::StreamConfiguration> &outputConfigs) {
    ALOGV("%s", __FUNCTION__);

    if (mServiceFatalErrorState) {
        ALOGE("%s: HDR+ service is in a fatal error state.", __FUNCTION__);
        return NO_INIT;
    }

    status_t res = mMessengerToService.configureStreams(inputConfig, outputConfigs);
    if (res == OK) {
        int64_t offset = inputConfig.isSensorInput ? inputConfig.sensorMode.timestampOffsetNs : 0;
        mApEaselMetadataManager.setApTimestampOffset(offset);
    }

    return res;
}

status_t HdrPlusClientImpl::setZslHdrPlusMode(bool enabled) {
    ALOGV("%s", __FUNCTION__);

    if (mServiceFatalErrorState) {
        ALOGE("%s: HDR+ service is in a fatal error state.", __FUNCTION__);
        return NO_INIT;
    }

    return mMessengerToService.setZslHdrPlusMode(enabled);
}

status_t HdrPlusClientImpl::submitCaptureRequest(pbcamera::CaptureRequest *request,
        const CameraMetadata &requestMetadata) {
  ATRACE_CALL();
  ALOGV("%s", __FUNCTION__);

    if (mServiceFatalErrorState) {
        ALOGE("%s: HDR+ service is in a fatal error state.", __FUNCTION__);
        return NO_INIT;
    }

    if (request == nullptr) {
        ALOGE("%s: request is nullptr.", __FUNCTION__);
        return BAD_VALUE;
    }

    pbcamera::RequestMetadata requestMetadataDest = {};
    status_t res = ApEaselMetadataManager::convertAndReturnRequestMetadata(&requestMetadataDest,
            requestMetadata);
    if (res != 0) {
        ALOGE("%s: Converting request metadata failed: %s (%d).", __FUNCTION__, strerror(-res),
                res);
        return res;
    }

    // Lock here to prevent the case where the result comes back very quickly and couldn't
    // find the request in mPendingRequests.
    Mutex::Autolock l(mPendingRequestsLock);

    PendingRequest pendingRequest;
    pendingRequest.request = *request;
    for (auto &outputBuffer : request->outputBuffers) {
        pendingRequest.outputBufferStatuses.emplace(outputBuffer.streamId, OUTPUT_BUFFER_REQUESTED);
    }

    START_PROFILER_TIMER(pendingRequest.timer);

    ATRACE_INT("PendingEaselCaptures", 1);

    // Send the request to HDR+ service.
    res = mMessengerToService.submitCaptureRequest(request, requestMetadataDest);
    if (res != 0) {
        ALOGE("%s: Sending capture request to service failed: %s (%d).", __FUNCTION__,
                strerror(-res), res);
        return res;
    }

    // Push the request to pending request queue to look up when HDR+ service returns the result.
    mPendingRequests.push_back(std::move(pendingRequest));

    return 0;
}

void HdrPlusClientImpl::notifyInputBuffer(const pbcamera::StreamBuffer &inputBuffer,
        int64_t timestampNs) {
    ALOGV("%s", __FUNCTION__);

    if (mServiceFatalErrorState) {
        ALOGE("%s: HDR+ service is in a fatal error state.", __FUNCTION__);
        return;
    }

    mMessengerToService.notifyInputBuffer(inputBuffer, timestampNs);
}

void HdrPlusClientImpl::notifyFrameMetadata(uint32_t frameNumber,
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
void HdrPlusClientImpl::notifyFrameEaselTimestamp(int64_t easelTimestampNs) {
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

void HdrPlusClientImpl::notifyServiceClosed() {
    // Return all pending requests.
    Mutex::Autolock listenerLock(mClientListenerLock);

    if (mClientListener != nullptr) {
        // If client listener is still valid, service is not closed by client.
        mServiceFatalErrorState = true;
        failAllPendingRequestsLocked();
        mClientListener->onFatalError();
    }
}

void HdrPlusClientImpl::notifyShutter(uint32_t requestId, int64_t apSensorTimestampNs) {
    ALOGV("%s: Got shutter callback for request %u timestamp %" PRId64, __FUNCTION__, requestId,
        apSensorTimestampNs);

    mClientListener->onShutter(requestId, apSensorTimestampNs);
}

void HdrPlusClientImpl::notifyNextCaptureReady(uint32_t requestId) {
    ALOGV("%s: Got next capture ready callback for request %u", __FUNCTION__, requestId);

    mClientListener->onNextCaptureReady(requestId);
}

status_t HdrPlusClientImpl::updateResultMetadata(std::shared_ptr<CameraMetadata> *cameraMetadata,
        const std::string &makernote) {
    if (cameraMetadata == nullptr || (*cameraMetadata) == nullptr) {
        ALOGE("%s: camera metadata is nullptr.", __FUNCTION__);
        return BAD_VALUE;
    }

    // Update maker note.
    (*cameraMetadata)->update(qcamera::NEXUS_EXPERIMENTAL_2017_EXIF_MAKERNOTE,
            reinterpret_cast<const uint8_t *>(makernote.c_str()), makernote.size());

    return OK;
}

void HdrPlusClientImpl::notifyDmaMakernote(pbcamera::DmaMakernote *dmaMakernote) {
    if (dmaMakernote == nullptr) return;
    if (dmaMakernote->dmaHandle == nullptr) {
        ALOGE("%s: DMA handle is nullptr.", __FUNCTION__);
        return;
    }

    ALOGV("%s: Received a makernote for request %d.", __FUNCTION__, dmaMakernote->requestId);

    Mutex::Autolock clientLock(mClientListenerLock);
    Mutex::Autolock requestLock(mPendingRequestsLock);

    // Find the pending request.
    for (auto &pendingRequest : mPendingRequests) {
        if (pendingRequest.request.id == dmaMakernote->requestId) {
            pendingRequest.makernote.resize(dmaMakernote->dmaMakernoteSize);
            status_t res = mMessengerToService.transferDmaBuffer(dmaMakernote->dmaHandle,
                    /*dmaBufFd*/-1, static_cast<void*>(&pendingRequest.makernote[0]),
                    dmaMakernote->dmaMakernoteSize);
            if (res != 0) {
                ALOGE("%s: Transferring makernote DMA buffer failed: %s (%d).", __FUNCTION__,
                        strerror(-res), res);
            }
            return;
        }
    }

    ALOGW("%s: Cannot find request %d for makernote.", __FUNCTION__, dmaMakernote->requestId);
}

void HdrPlusClientImpl::notifyDmaPostview(uint32_t requestId, void *dmaHandle, uint32_t width,
            uint32_t height, uint32_t stride, int32_t format) {

    ALOGI("%s: Received a postview %dx%d for request %d stride %d", __FUNCTION__, width, height,
            requestId, stride);

    uint32_t dataSize = stride * height;
    auto postview = std::make_unique<std::vector<uint8_t>>(dataSize);

    status_t res = mMessengerToService.transferDmaBuffer(dmaHandle,
            /*dmaBufFd*/-1, postview.get()->data(), dataSize);

    if (res != OK) {
        ALOGE("%s: Transfering DMA buffer failed: %s (%d).", __FUNCTION__, strerror(-res), res);
        return;
    }

    {
        Mutex::Autolock clientLock(mClientListenerLock);
        if (mClientListener != nullptr) {
            mClientListener->onPostview(requestId, std::move(postview), width, height, stride,
                    format);
        }
    }
}

status_t HdrPlusClientImpl::createFileDumpDirectory(const std::string &baseDir,
        const std::vector<std::string>& paths, std::string *finalPath) {
    std::string path = baseDir;
    for (uint i = 0; i < paths.size() - 1; i++) {
        path.append("/" + paths[i]);
        status_t res = createDir(path);
        if (res != 0) {
            ALOGE("%s: createDir (%s) failed: %s (%d)", __FUNCTION__,
                path.c_str(), strerror(-res), res);
            return res;
        }
    }

    if (finalPath != nullptr) {
        path.append("/" + paths[paths.size() - 1]);
        *finalPath = path;
    }

    return OK;
}

status_t HdrPlusClientImpl::createDir(const std::string &dir) {
    // Check if the directory exists.
    DIR *d = opendir(dir.c_str());
    if (d != nullptr) {
        // Directory exists.
        closedir(d);
        return OK;
    } else if (errno == ENOENT) {
        // Directory doesn't exist, create it.
        if (mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
            ALOGE("%s: Creating directory (%s) failed: %s (%d)", __FUNCTION__,
                    dir.c_str(), strerror(errno), -errno);
            return -errno;
        }
        return OK;
    } else {
        ALOGE("%s: opendir failed: %s (%d)", __FUNCTION__, strerror(errno), -errno);
        return -errno;
    }
}

std::vector<std::string> HdrPlusClientImpl::splitPath(const std::string &filename) {
    // split filename, separated by "/", into strings.
    std::vector<std::string> paths;
    std::string::size_type left = 0, right = 0;
    while (1) {
        if (filename.substr(left).size() == 0) {
            break;
        }

        right = filename.find('/', left);
        if (right == std::string::npos) {
            // No more "/" found in the substring.
            if (filename.substr(left).size() > 0) {
                paths.push_back(filename.substr(left));
            }
            break;
        } else if (right - left == 0) {
            // right and left both point to "/", ignore this character.
            left++;
        } else {
            // right points to "/". Push the directory name between left and right to paths.
            paths.push_back(filename.substr(left, right - left));
            left = right + 1;
        }
    }

    return paths;
}

void HdrPlusClientImpl::writeData(const std::string& path,
        std::vector<char> &data) {
    std::ofstream outfile(path, std::ios::binary);
    if (!outfile.is_open()) {
        ALOGE("%s: Opening file (%s) failed.", __FUNCTION__, path.c_str());
        return;
    }

    outfile.write(&data[0], data.size());
    outfile.close();
}

void HdrPlusClientImpl::notifyDmaFileDump(const std::string &filename, DmaBufferHandle dmaHandle,
        uint32_t dmaDataSize) {

    static const std::string kDumpDirectory("/data/vendor/camera");
    std::vector<char> data(dmaDataSize);
    status_t res;

    {
        Mutex::Autolock clientLock(mClientListenerLock);
        res = mMessengerToService.transferDmaBuffer(dmaHandle, /*dmaBufFd*/-1, &data[0],
                data.size());
        if (res != 0) {
            ALOGE("%s: Transferring a file (%s) dump failed: %s (%d)", __FUNCTION__,
                    filename.c_str(), strerror(-res), res);
            return;
        }
    }

    // Split path.
    std::vector<std::string> paths = splitPath(filename);
    if (paths.size() == 0) {
        ALOGE("%s: Cannot save to %s", __FUNCTION__, filename.c_str());
        return;
    }

    // Create the directory for the file.
    std::string finalPath;
    res = createFileDumpDirectory(kDumpDirectory, paths, &finalPath);
    if (res != 0) {
        ALOGE("%s: Creating file dump directory (%s) failed: %s (%d)", __FUNCTION__,
                filename.c_str(), strerror(-res), res);
        return;
    }

    // Write data to the file.
    writeData(finalPath, data);

    ALOGD("%s: Dump data to file: %s", __FUNCTION__, finalPath.c_str());
}

void HdrPlusClientImpl::notifyDmaCaptureResult(pbcamera::DmaCaptureResult *result) {
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
                if (requestBuffer->streamId != result->buffer.streamId) continue;

                // Found the output buffer. Now transfer the content of DMA buffer to this
                // output buffer.
                status_t res = mMessengerToService.transferDmaBuffer(result->buffer.dmaHandle,
                        requestBuffer->dmaBufFd, requestBuffer->data, requestBuffer->dataSize);

                if (res != 0) {
                    ALOGE("%s: Transferring DMA buffer failed: %s (%d).", __FUNCTION__,
                            strerror(-res), res);
                    successfulResult = false;
                }

                std::unordered_map<uint32_t, OutputBufferStatus> *bufferStatuses =
                        &mPendingRequests[i].outputBufferStatuses;

                // Update output buffer status.
                auto bufferStatus = bufferStatuses->find(requestBuffer->streamId);
                if (bufferStatus != bufferStatuses->end()) {
                    if (bufferStatus->second != OUTPUT_BUFFER_REQUESTED) {
                        ALOGW("%s: Aleady received result for request %d stream %d",
                                __FUNCTION__, result->requestId, result->buffer.streamId);
                    }
                    bufferStatus->second = successfulResult ?
                            OUTPUT_BUFFER_CAPTURED : OUTPUT_BUFFER_FAILED;
                } else {
                    ALOGW("%s: Cannot find output buffer status for stream %d", __FUNCTION__,
                            requestBuffer->streamId);
                }

                // Check if all results are back.
                for (auto &status : *bufferStatuses) {
                    if (status.second == OUTPUT_BUFFER_REQUESTED) {
                        // Return if not all output buffers in this request are back.
                        return;
                    } else if (status.second == OUTPUT_BUFFER_FAILED) {
                        successfulResult = false;
                    }
                }

                // All output buffers in this request are back, ready to send capture result.
                ATRACE_INT("PendingEaselCaptures", 0);
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
                    res = updateResultMetadata(&cameraMetadata, mPendingRequests[i].makernote);
                    if (res != OK) {
                        ALOGE("%s: Failed to update result metadata.", __FUNCTION__);
                        successfulResult = false;
                    } else {
                        resultMetadata = cameraMetadata->getAndLock();
                    }
                }

                pbcamera::CaptureResult clientResult = {};
                clientResult.requestId = result->requestId;
                clientResult.outputBuffers = mPendingRequests[i].request.outputBuffers;

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

                // Remove the pending request.
                auto requestIter = mPendingRequests.begin() + i;
                mPendingRequests.erase(requestIter);

                return;
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
