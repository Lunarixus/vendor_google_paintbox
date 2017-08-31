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
#define LOG_TAG "MessengerToHdrPlusService"
#include <log/log.h>

#include "MessengerToHdrPlusService.h"
#include "HdrPlusMessageTypes.h"

namespace pbcamera {

MessengerToHdrPlusService::MessengerToHdrPlusService() : mConnected(false) {
}

MessengerToHdrPlusService::~MessengerToHdrPlusService() {
    disconnect();
}

#define RETURN_ON_WRITE_ERROR(_expr) \
    do { \
        status_t res = (_expr); \
        if (res != 0) { \
            returnMessage(message); \
            ALOGE("%s: writing message failed: %s (%d)", __FUNCTION__, strerror(-res), res); \
            return; \
        } \
    } while(0)

#define RETURN_ERROR_ON_WRITE_ERROR(_expr) \
    do { \
        status_t res = (_expr); \
        if (res != 0) { \
            returnMessage(message); \
            ALOGE("%s: writing message failed: %s (%d)", __FUNCTION__, strerror(-res), res); \
            return res; \
        } \
    } while(0)

status_t MessengerToHdrPlusService::connect(EaselMessengerListener &listener) {
    std::lock_guard<std::mutex> lock(mApiLock);

    if (mConnected) return -EEXIST;

    status_t res;

    res = mEaselCommClient.open(EASEL_SERVICE_HDRPLUS);
    if (res != 0) {
        ALOGE("%s: Opening EaselComm failed: %s (%d)", __FUNCTION__, strerror(-res), res);
        return res;
    }

    // Connect to EaselComm
    res = EaselMessenger::connect(listener, kMaxHdrPlusMessageSize, &mEaselCommClient);
    if (res != 0) {
        ALOGE("%s: Connecting to EaselComm failed: %s (%d)", __FUNCTION__, strerror(-res), res);
        mEaselCommClient.close();
        return res;
    }

    mConnected = true;

    // Connect to HDR+ Service
    res = connectToService();
    if (res != 0) {
        ALOGE("%s: Connecting to HDR+ service failed: %s (%d)", __FUNCTION__, strerror(-res), res);
        disconnectLocked();
        return res;
    }

    return 0;
}

status_t MessengerToHdrPlusService::connectToService() {
    // Prepare the message.
    Message *message = nullptr;
    status_t res = getEmptyMessage(&message);
    if (res != 0) return res;

    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(MESSAGE_CONNECT));

    // Send to service.
    return sendMessage(message);
}

void MessengerToHdrPlusService::disconnect() {
    std::lock_guard<std::mutex> lock(mApiLock);
    return disconnectLocked();
}

status_t MessengerToHdrPlusService::disconnectFromService() {
    // Prepare the message.
    Message *message = nullptr;
    int res = getEmptyMessage(&message);
    if (res != 0) return res;

    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(MESSAGE_DISCONNECT));

    // Send to service.
    return sendMessage(message);
}

void MessengerToHdrPlusService::disconnectLocked() {
    if (!mConnected) return;

    int res = disconnectFromService();
    if (res != 0) {
        ALOGE("%s: Disconnecting from service failed: %s (%d).", __FUNCTION__, strerror(-res), res);
    }
    mEaselCommClient.close();
    EaselMessenger::disconnect();
    mConnected = false;
}

status_t MessengerToHdrPlusService::setStaticMetadata(const StaticMetadata& metadata) {
    std::lock_guard<std::mutex> lock(mApiLock);
    if (!mConnected) {
        ALOGE("%s: Not connected to service.", __FUNCTION__);
        return -ENODEV;
    }

    // Prepare the message.
    Message *message = nullptr;
    status_t res = getEmptyMessage(&message);
    if (res != 0) return res;

    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(MESSAGE_SET_STATIC_METADATA));

    // Serialize StaticMetadata
    RETURN_ERROR_ON_WRITE_ERROR(message->writeByte(metadata.flashInfoAvailable));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeInt32Array(metadata.sensitivityRange));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeInt32(metadata.maxAnalogSensitivity));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeInt32Array(metadata.pixelArraySize));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeInt32Array(metadata.activeArraySize));

    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(metadata.opticalBlackRegions.size()));
    for (size_t i = 0; i < metadata.opticalBlackRegions.size(); i++) {
        RETURN_ERROR_ON_WRITE_ERROR(message->writeInt32Array(metadata.opticalBlackRegions[i]));
    }

    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(
        metadata.availableStreamConfigurations.size()));
    for (size_t i = 0; i < metadata.availableStreamConfigurations.size(); i++) {
        RETURN_ERROR_ON_WRITE_ERROR(message->writeInt32Array(
                metadata.availableStreamConfigurations[i]));
    }

    RETURN_ERROR_ON_WRITE_ERROR(message->writeByte(metadata.referenceIlluminant1));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeByte(metadata.referenceIlluminant2));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeFloatArray(metadata.calibrationTransform1));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeFloatArray(metadata.calibrationTransform2));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeFloatArray(metadata.colorTransform1));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeFloatArray(metadata.colorTransform2));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeInt32(metadata.whiteLevel));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeByte(metadata.colorFilterArrangement));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeFloatVector(metadata.availableApertures));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeFloatVector(metadata.availableFocalLengths));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeInt32Array(metadata.shadingMapSize));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeByte(metadata.focusDistanceCalibration));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(metadata.debugParams));

    // Send to service.
    return sendMessage(message);
}

status_t MessengerToHdrPlusService::writeStreamConfiguration(Message *message,
        const StreamConfiguration &config) {
    if (message == nullptr) return -ENODEV;

    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(config.id));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(config.image.width));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(config.image.height));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeInt32(config.image.format));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(config.image.padding));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(config.image.planes.size()));

    for (auto &plane : config.image.planes) {
        RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(plane.stride));
        RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(plane.scanline));
    }

    return 0;
}

status_t MessengerToHdrPlusService::configureStreams(
            const pbcamera::InputConfiguration &inputConfig,
            const std::vector<StreamConfiguration> &outputConfigs) {
    ALOGV("%s", __FUNCTION__);

    if (outputConfigs.size() == 0) return -EINVAL;

    std::lock_guard<std::mutex> lock(mApiLock);
    if (!mConnected) return -ENODEV;

    // Prepare the message.
    Message *message = nullptr;
    status_t res = getEmptyMessage(&message);
    if (res != 0) return res;

    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(MESSAGE_CONFIGURE_STREAMS));

    // Serialize InputConfiguration
    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(inputConfig.isSensorInput));
    if (inputConfig.isSensorInput) {
        // Serialize sensor mode
        RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(
                inputConfig.sensorMode.cameraId));
        RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(
                inputConfig.sensorMode.pixelArrayWidth));
        RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(
                inputConfig.sensorMode.pixelArrayHeight));
        RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(
                inputConfig.sensorMode.activeArrayWidth));
        RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(
                inputConfig.sensorMode.activeArrayHeight));
        RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(
                inputConfig.sensorMode.outputPixelClkHz));
        RETURN_ERROR_ON_WRITE_ERROR(message->writeInt32(
                inputConfig.sensorMode.format));
    } else {
        // Serialize input stream config
        res = writeStreamConfiguration(message, inputConfig.streamConfig);
        if (res != 0) return res;
    }

    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(outputConfigs.size()));
    for (auto &outputConfig : outputConfigs) {
        res = writeStreamConfiguration(message, outputConfig);
        if (res != 0) return res;
    }

    // Send to service.
    return sendMessage(message);
}

status_t MessengerToHdrPlusService::setZslHdrPlusMode(bool enabled) {
    std::lock_guard<std::mutex> lock(mApiLock);
    if (!mConnected) return -ENODEV;

    // Prepare the message.
    Message *message = nullptr;
    status_t res = getEmptyMessage(&message);
    if (res != 0) return res;

    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(MESSAGE_SET_ZSL_HDR_PLUS_MODE));

    // Serialize ZSl HDR+ mode.
    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(enabled));

    return sendMessage(message);
}

status_t MessengerToHdrPlusService::submitCaptureRequest(CaptureRequest *request,
        const RequestMetadata &metadata) {
    if (request == nullptr) return -EINVAL;

    std::lock_guard<std::mutex> lock(mApiLock);
    if (!mConnected) return -ENODEV;

    // Prepare the message.
    Message *message = nullptr;
    status_t res = getEmptyMessage(&message);
    if (res != 0) return res;

    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(MESSAGE_SUBMIT_CAPTURE_REQUEST));

    // Serialize capture request.
    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(request->id));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(request->outputBuffers.size()));
    for (uint32_t i = 0; i < request->outputBuffers.size(); i++) {
        RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(request->outputBuffers[i].streamId));
        // Skip request->outputBuffers[i].data
    }

    // Serialize request metadata.
    RETURN_ERROR_ON_WRITE_ERROR(message->writeInt32Array(metadata.cropRegion));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(metadata.postviewEnable));

    return sendMessage(message);
}

void MessengerToHdrPlusService::notifyInputBuffer(const StreamBuffer &inputBuffer,
        int64_t timestampNs) {
    std::lock_guard<std::mutex> lock(mApiLock);
    if (!mConnected) {
        ALOGE("%s: Not connected to service.", __FUNCTION__);
        return;
    }

    // Prepare the message.
    Message *message = nullptr;
    status_t res = getEmptyMessage(&message);
    if (res != 0) {
        ALOGE("%s: Getting an empty message failed: %s (%d).", __FUNCTION__, strerror(-res), res);
        return;
    }

    RETURN_ON_WRITE_ERROR(message->writeUint32(MESSAGE_NOTIFY_DMA_INPUT_BUFFER));

    // Serialize input buffer and timestamp
    RETURN_ON_WRITE_ERROR(message->writeUint32(inputBuffer.streamId));
    RETURN_ON_WRITE_ERROR(message->writeInt64(timestampNs));

    res = sendMessageWithDmaBuffer(message, inputBuffer.data, inputBuffer.dataSize,
            inputBuffer.dmaBufFd);
    if (res != 0) {
        ALOGE("%s: Sending a message with DMA buffer failed: %s (%d).", __FUNCTION__,
                strerror(-res), res);
    }
}

void MessengerToHdrPlusService::notifyFrameMetadataAsync(const FrameMetadata &metadata) {
    std::lock_guard<std::mutex> lock(mApiLock);
    if (!mConnected) {
        ALOGE("%s: Not connected to service.", __FUNCTION__);
        return;
    }

    // Prepare the message.
    Message *message = nullptr;
    status_t res = getEmptyMessage(&message);
    if (res != 0) {
        ALOGE("%s: Getting an empty message failed: %s (%d).", __FUNCTION__, strerror(-res), res);
        return;
    }

    RETURN_ON_WRITE_ERROR(message->writeUint32(MESSAGE_NOTIFY_FRAME_METADATA_ASYNC));

    // Serialize FrameMetadata
    RETURN_ON_WRITE_ERROR(message->writeInt64(metadata.easelTimestamp));
    RETURN_ON_WRITE_ERROR(message->writeInt64(metadata.exposureTime));
    RETURN_ON_WRITE_ERROR(message->writeInt32(metadata.sensitivity));
    RETURN_ON_WRITE_ERROR(message->writeInt32(metadata.postRawSensitivityBoost));
    RETURN_ON_WRITE_ERROR(message->writeByte(metadata.flashMode));
    RETURN_ON_WRITE_ERROR(message->writeFloatArray(metadata.colorCorrectionGains));
    RETURN_ON_WRITE_ERROR(message->writeFloatArray(metadata.colorCorrectionTransform));
    RETURN_ON_WRITE_ERROR(message->writeFloatArray(metadata.neutralColorPoint));
    RETURN_ON_WRITE_ERROR(message->writeInt64(metadata.timestamp));
    RETURN_ON_WRITE_ERROR(message->writeByte(metadata.blackLevelLock));
    RETURN_ON_WRITE_ERROR(message->writeByte(metadata.faceDetectMode));
    RETURN_ON_WRITE_ERROR(message->writeInt32Vector(metadata.faceIds));

    RETURN_ON_WRITE_ERROR(message->writeUint32(metadata.faceLandmarks.size()));
    for (size_t i = 0; i < metadata.faceLandmarks.size(); i++) {
        RETURN_ON_WRITE_ERROR(message->writeInt32Array(metadata.faceLandmarks[i]));
    }

    RETURN_ON_WRITE_ERROR(message->writeUint32(metadata.faceRectangles.size()));
    for (size_t i = 0; i < metadata.faceRectangles.size(); i++) {
        RETURN_ON_WRITE_ERROR(message->writeInt32Array(metadata.faceRectangles[i]));
    }
    RETURN_ON_WRITE_ERROR(message->writeByteVector(metadata.faceScores));
    RETURN_ON_WRITE_ERROR(message->writeByte(metadata.sceneFlicker));

    RETURN_ON_WRITE_ERROR(message->writeUint32(metadata.noiseProfile.size()));
    for (size_t i = 0; i < metadata.noiseProfile.size(); i++) {
        RETURN_ON_WRITE_ERROR(message->writeDoubleArray(metadata.noiseProfile[i]));
    }

    RETURN_ON_WRITE_ERROR(message->writeFloatArray(metadata.dynamicBlackLevel));
    RETURN_ON_WRITE_ERROR(message->writeFloatVector(metadata.lensShadingMap));
    RETURN_ON_WRITE_ERROR(message->writeFloat(metadata.focusDistance));

    res = sendMessage(message, /*async*/true);
    if (res != 0) {
        ALOGE("%s: Sending a message failed: %s (%d).", __FUNCTION__, strerror(-res), res);
    }
}

} // namespace pbcamera
