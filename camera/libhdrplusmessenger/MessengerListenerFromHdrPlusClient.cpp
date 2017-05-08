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
#define LOG_TAG "MessengerListenerFromHdrPlusClient"
#include <utils/Log.h>

#include "HdrPlusMessageTypes.h"
#include "MessengerListenerFromHdrPlusClient.h"

namespace pbcamera {

#define RETURN_ON_READ_ERROR(_expr) \
    do { \
        status_t res = (_expr); \
        if (res != 0) { \
            ALOGE("%s: reading message failed: %s (%d)", __FUNCTION__, strerror(-res), res); \
            return; \
        } \
    } while(0)

#define RETURN_ERROR_ON_READ_ERROR(_expr) \
    do { \
        status_t res = (_expr); \
        if (res != 0) { \
            ALOGE("%s: %d: reading message failed: %s (%d)", __FUNCTION__, __LINE__, \
                    strerror(-res), res); \
            return res; \
        } \
    } while(0)

status_t MessengerListenerFromHdrPlusClient::onMessage(Message *message) {
    if (message == nullptr) return -EINVAL;

    uint32_t type = 0;

    RETURN_ERROR_ON_READ_ERROR(message->readUint32(&type));
    ALOGV("%s: Got message %u", __FUNCTION__, type);
    switch(type) {
        case MESSAGE_CONNECT:
            return connect();
        case MESSAGE_DISCONNECT:
            disconnect();
            return 0;
        case MESSAGE_SET_STATIC_METADATA:
            return deserializeSetStaticMetadata(message);
        case MESSAGE_CONFIGURE_STREAMS:
            return deserializeConfigureStreams(message);
        case MESSAGE_SET_ZSL_HDR_PLUS_MODE:
            return deserializeSetZslHdrPlusMode(message);
        case MESSAGE_SUBMIT_CAPTURE_REQUEST:
            return deserializeSubmitCaptureRequest(message);
        case MESSAGE_NOTIFY_FRAME_METADATA_ASYNC:
            deserializeNotifyFrameMetadata(message);
            return 0;
        default:
            ALOGE("%s: Received invalid message type %d.", __FUNCTION__, type);
            return -EINVAL;
    }

    return 0;
}

status_t MessengerListenerFromHdrPlusClient::onMessageWithDmaBuffer(Message *message,
        DmaBufferHandle handle, uint32_t dmaBufferSize) {
    (void)handle;
    (void)dmaBufferSize;
    if (message == nullptr) return -EINVAL;

    uint32_t type = 0;
    RETURN_ERROR_ON_READ_ERROR(message->readUint32(&type));
    ALOGV("%s: Got message %u", __FUNCTION__, type);

    switch(type) {
        case MESSAGE_NOTIFY_DMA_INPUT_BUFFER:
            deserializeNotifyDmaInputBuffer(message, handle, dmaBufferSize);
            return 0;
        default:
            ALOGE("%s: Received invalid message type %d.", __FUNCTION__, type);
            return -EINVAL;
    }

    return 0;
}

void MessengerListenerFromHdrPlusClient::onEaselCommClosed() {
    notifyClientClosed();
}

status_t MessengerListenerFromHdrPlusClient::deserializeConnect(Message *message){
    (void)message;
    return connect();
}

status_t MessengerListenerFromHdrPlusClient::deserializeSetStaticMetadata(Message *message) {
    StaticMetadata metadata = {};

    // Serialize StaticMetadata
    RETURN_ERROR_ON_READ_ERROR(message->readByte(&metadata.flashInfoAvailable));
    RETURN_ERROR_ON_READ_ERROR(message->readInt32Array(&metadata.sensitivityRange));
    RETURN_ERROR_ON_READ_ERROR(message->readInt32(&metadata.maxAnalogSensitivity));
    RETURN_ERROR_ON_READ_ERROR(message->readInt32Array(&metadata.pixelArraySize));
    RETURN_ERROR_ON_READ_ERROR(message->readInt32Array(&metadata.activeArraySize));

    uint32_t vectorSize;
    RETURN_ERROR_ON_READ_ERROR(message->readUint32(&vectorSize));
    metadata.opticalBlackRegions.resize(vectorSize);
    for (size_t i = 0; i < vectorSize; i++) {
        RETURN_ERROR_ON_READ_ERROR(message->readInt32Array(&metadata.opticalBlackRegions[i]));
    }

    RETURN_ERROR_ON_READ_ERROR(message->readUint32(&vectorSize));
    metadata.availableStreamConfigurations.resize(vectorSize);
    for (size_t i = 0; i < vectorSize; i++) {
        RETURN_ERROR_ON_READ_ERROR(message->readInt32Array(
                &metadata.availableStreamConfigurations[i]));
    }

    RETURN_ERROR_ON_READ_ERROR(message->readByte(&metadata.referenceIlluminant1));
    RETURN_ERROR_ON_READ_ERROR(message->readByte(&metadata.referenceIlluminant2));
    RETURN_ERROR_ON_READ_ERROR(message->readFloatArray(&metadata.calibrationTransform1));
    RETURN_ERROR_ON_READ_ERROR(message->readFloatArray(&metadata.calibrationTransform2));
    RETURN_ERROR_ON_READ_ERROR(message->readFloatArray(&metadata.colorTransform1));
    RETURN_ERROR_ON_READ_ERROR(message->readFloatArray(&metadata.colorTransform2));
    RETURN_ERROR_ON_READ_ERROR(message->readInt32(&metadata.whiteLevel));
    RETURN_ERROR_ON_READ_ERROR(message->readByte(&metadata.colorFilterArrangement));
    RETURN_ERROR_ON_READ_ERROR(message->readFloatVector(&metadata.availableApertures));
    RETURN_ERROR_ON_READ_ERROR(message->readFloatVector(&metadata.availableFocalLengths));
    RETURN_ERROR_ON_READ_ERROR(message->readInt32Array(&metadata.shadingMapSize));
    RETURN_ERROR_ON_READ_ERROR(message->readByte(&metadata.focusDistanceCalibration));

    return setStaticMetadata(metadata);
}

status_t MessengerListenerFromHdrPlusClient::readStreamConfiguration(Message *message,
        StreamConfiguration *config) {
    if (message == nullptr || config == nullptr) return -ENODEV;

    uint32_t numPlanes = 0;

    RETURN_ERROR_ON_READ_ERROR(message->readUint32(&config->id));
    RETURN_ERROR_ON_READ_ERROR(message->readUint32(&config->image.width));
    RETURN_ERROR_ON_READ_ERROR(message->readUint32(&config->image.height));
    RETURN_ERROR_ON_READ_ERROR(message->readInt32(&config->image.format));
    RETURN_ERROR_ON_READ_ERROR(message->readUint32(&config->image.padding));
    RETURN_ERROR_ON_READ_ERROR(message->readUint32(&numPlanes));
    for (uint32_t i = 0; i < numPlanes; i++) {
        PlaneConfiguration plane;
        RETURN_ERROR_ON_READ_ERROR(message->readUint32(&plane.stride));
        RETURN_ERROR_ON_READ_ERROR(message->readUint32(&plane.scanline));
        config->image.planes.push_back(plane);
    }

    return 0;
}

status_t MessengerListenerFromHdrPlusClient::deserializeConfigureStreams(Message *message){
    InputConfiguration inputConfig;
    std::vector<StreamConfiguration> outputConfigs;
    uint32_t numOutputConfigs = 0;

    // Deserialize InputConfiguration
    uint32_t isSensorInput = 0;
    RETURN_ERROR_ON_READ_ERROR(message->readUint32(&isSensorInput));
    inputConfig.isSensorInput = (isSensorInput != 0);
    if (inputConfig.isSensorInput) {
        // Deserialize sensor mode
        RETURN_ERROR_ON_READ_ERROR(message->readUint32(
                &inputConfig.sensorMode.cameraId));
        RETURN_ERROR_ON_READ_ERROR(message->readUint32(
                &inputConfig.sensorMode.pixelArrayWidth));
        RETURN_ERROR_ON_READ_ERROR(message->readUint32(
                &inputConfig.sensorMode.pixelArrayHeight));
        RETURN_ERROR_ON_READ_ERROR(message->readUint32(
                &inputConfig.sensorMode.activeArrayWidth));
        RETURN_ERROR_ON_READ_ERROR(message->readUint32(
                &inputConfig.sensorMode.activeArrayHeight));
        RETURN_ERROR_ON_READ_ERROR(message->readUint32(
                &inputConfig.sensorMode.outputPixelClkHz));
        RETURN_ERROR_ON_READ_ERROR(message->readInt32(
                &inputConfig.sensorMode.format));
    } else {
        // Deserialize input stream configuration.
        status_t res = readStreamConfiguration(message, &inputConfig.streamConfig);
        if (res != 0) return res;
    }

    RETURN_ERROR_ON_READ_ERROR(message->readUint32(&numOutputConfigs));
    for (uint32_t i = 0; i < numOutputConfigs; i++) {
        StreamConfiguration outputConfig = {};
        status_t res = readStreamConfiguration(message, &outputConfig);
        if (res != 0) return res;

        outputConfigs.push_back(outputConfig);
    }

    return configureStreams(inputConfig, outputConfigs);
}

status_t MessengerListenerFromHdrPlusClient::deserializeSetZslHdrPlusMode(Message *message) {
    // Deserialize InputConfiguration
    uint32_t enabled = 0;

    RETURN_ERROR_ON_READ_ERROR(message->readUint32(&enabled));
    return setZslHdrPlusMode(enabled != 0);
}

status_t MessengerListenerFromHdrPlusClient::deserializeSubmitCaptureRequest(Message *message){
    CaptureRequest request = {};
    uint32_t numOutputBuffers = 0;

    // Deserialize CaptureRequest
    RETURN_ERROR_ON_READ_ERROR(message->readUint32(&request.id));
    RETURN_ERROR_ON_READ_ERROR(message->readUint32(&numOutputBuffers));
    for (uint32_t i = 0; i < numOutputBuffers; i++) {
        StreamBuffer buffer = {};
        RETURN_ERROR_ON_READ_ERROR(message->readUint32(&buffer.streamId));
        // buffer.data remains nullptr because it's ignored in the service.
        request.outputBuffers.push_back(buffer);
    }

    return submitCaptureRequest(request);
}

void MessengerListenerFromHdrPlusClient::deserializeNotifyDmaInputBuffer(Message *message,
        DmaBufferHandle dmaHandle, int dmaDataSize) {
    DmaImageBuffer dmaImageBuffer = {};
    int64_t timestampNs;

    // Deserialize input buffer and timestamp.
    RETURN_ON_READ_ERROR(message->readUint32(&dmaImageBuffer.streamId));
    RETURN_ON_READ_ERROR(message->readInt64(&timestampNs));

    dmaImageBuffer.dmaHandle = dmaHandle;
    dmaImageBuffer.dmaDataSize = dmaDataSize;

    notifyDmaInputBuffer(dmaImageBuffer, timestampNs);
}

void MessengerListenerFromHdrPlusClient::deserializeNotifyFrameMetadata(Message *message) {
    FrameMetadata metadata = {};

    // Deserialize FrameMetadata
    RETURN_ON_READ_ERROR(message->readInt64(&metadata.easelTimestamp));
    RETURN_ON_READ_ERROR(message->readInt64(&metadata.exposureTime));
    RETURN_ON_READ_ERROR(message->readInt32(&metadata.sensitivity));
    RETURN_ON_READ_ERROR(message->readInt32(&metadata.postRawSensitivityBoost));
    RETURN_ON_READ_ERROR(message->readByte(&metadata.flashMode));
    RETURN_ON_READ_ERROR(message->readFloatArray(&metadata.colorCorrectionGains));
    RETURN_ON_READ_ERROR(message->readFloatArray(&metadata.colorCorrectionTransform));
    RETURN_ON_READ_ERROR(message->readFloatArray(&metadata.neutralColorPoint));
    RETURN_ON_READ_ERROR(message->readInt64(&metadata.timestamp));
    RETURN_ON_READ_ERROR(message->readByte(&metadata.blackLevelLock));
    RETURN_ON_READ_ERROR(message->readByte(&metadata.faceDetectMode));
    RETURN_ON_READ_ERROR(message->readInt32Vector(&metadata.faceIds));

    uint32_t vectorSize = 0;
    RETURN_ON_READ_ERROR(message->readUint32(&vectorSize));
    metadata.faceLandmarks.resize(vectorSize);
    for (size_t i = 0; i < metadata.faceLandmarks.size(); i++) {
        RETURN_ON_READ_ERROR(message->readInt32Array(&metadata.faceLandmarks[i]));
    }

    RETURN_ON_READ_ERROR(message->readUint32(&vectorSize));
    metadata.faceRectangles.resize(vectorSize);
    for (size_t i = 0; i < metadata.faceRectangles.size(); i++) {
        RETURN_ON_READ_ERROR(message->readInt32Array(&metadata.faceRectangles[i]));
    }

    RETURN_ON_READ_ERROR(message->readByteVector(&metadata.faceScores));
    RETURN_ON_READ_ERROR(message->readByte(&metadata.sceneFlicker));

    RETURN_ON_READ_ERROR(message->readUint32(&vectorSize));
    for (size_t i = 0; i < metadata.noiseProfile.size(); i++) {
        RETURN_ON_READ_ERROR(message->readDoubleArray(&metadata.noiseProfile[i]));
    }

    RETURN_ON_READ_ERROR(message->readFloatArray(&metadata.dynamicBlackLevel));
    RETURN_ON_READ_ERROR(message->readFloatVector(&metadata.lensShadingMap));
    RETURN_ON_READ_ERROR(message->readFloat(&metadata.focusDistance));

    notifyFrameMetadata(metadata);
}

} // namespace pbcamera
