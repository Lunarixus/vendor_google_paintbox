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
#define LOG_TAG "MessengerListenerFromHdrPlusService"
#include <utils/Log.h>

#include "HdrPlusMessageTypes.h"
#include "MessengerListenerFromHdrPlusService.h"

namespace pbcamera {

#define RETURN_ERROR_ON_READ_ERROR(_expr) \
    do { \
        status_t res = (_expr); \
        if (res != 0) { \
            ALOGE("%s: reading message failed: %s (%d)", __FUNCTION__, strerror(-res), res); \
            return res; \
        } \
    } while(0)

#define RETURN_ON_READ_ERROR(_expr) \
    do { \
        status_t res = (_expr); \
        if (res != 0) { \
            ALOGE("%s: reading message failed: %s (%d)", __FUNCTION__, strerror(-res), res); \
            return; \
        } \
    } while(0)


status_t MessengerListenerFromHdrPlusService::onMessage(Message *message) {
    if (message == nullptr) return -EINVAL;

    uint32_t type = 0;
    RETURN_ERROR_ON_READ_ERROR(message->readUint32(&type));

    switch(type) {
        case MESSAGE_NOTIFY_FRAME_EASEL_TIMESTAMP_ASYNC:
            deserializeNotifyFrameEaselTimestamp(message);
            return 0;
        case MESSAGE_NOTIFY_SHUTTER_ASYNC:
            deserializeNotifyShutter(message);
            return 0;
        default:
            ALOGE("%s: Receive invalid message type %d.", __FUNCTION__, type);
            return -EINVAL;
    }

    return 0;
}

status_t MessengerListenerFromHdrPlusService::onMessageWithDmaBuffer(Message *message,
        DmaBufferHandle handle, uint32_t dmaBufferSize) {
    if (message == nullptr) return -EINVAL;

    uint32_t type = 0;
    RETURN_ERROR_ON_READ_ERROR(message->readUint32(&type));

    switch(type) {
        case MESSAGE_NOTIFY_DMA_CAPTURE_RESULT:
            deserializeNotifyDmaCaptureResult(message, handle, dmaBufferSize);
            return 0;
        default:
            ALOGE("%s: Receive invalid message type %d.", __FUNCTION__, type);
            return -EINVAL;
    }

    return 0;
}

void MessengerListenerFromHdrPlusService::deserializeNotifyFrameEaselTimestamp(Message *message) {
    int64_t easelTimestampNs = 0;
    RETURN_ON_READ_ERROR(message->readInt64(&easelTimestampNs));

    notifyFrameEaselTimestamp(easelTimestampNs);
}

void MessengerListenerFromHdrPlusService::deserializeNotifyShutter(Message *message) {
    uint32_t requestId = 0;
    int64_t apSensorTimestampNs = 0;
    RETURN_ON_READ_ERROR(message->readUint32(&requestId));
    RETURN_ON_READ_ERROR(message->readInt64(&apSensorTimestampNs));

    notifyShutter(requestId, apSensorTimestampNs);
}

void MessengerListenerFromHdrPlusService::deserializeNotifyDmaCaptureResult(Message *message,
        DmaBufferHandle dmaHandle, int dmaDataSize) {

    DmaCaptureResult result = {};

    RETURN_ON_READ_ERROR(message->readUint32(&result.requestId));
    RETURN_ON_READ_ERROR(message->readUint32(&result.buffer.streamId));
    RETURN_ON_READ_ERROR(message->readInt64(&result.metadata.easelTimestamp));
    RETURN_ON_READ_ERROR(message->readInt64(&result.metadata.timestamp));

    result.buffer.dmaHandle = dmaHandle;
    result.buffer.dmaDataSize = dmaDataSize;

    notifyDmaCaptureResult(&result);
}

} // namespace pbcamera
