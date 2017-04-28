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
#define LOG_TAG "MessengerToHdrPlusClient"
#include <utils/Log.h>

#include "MessengerToHdrPlusClient.h"

namespace pbcamera {

#define RETURN_ON_WRITE_ERROR(_expr) \
    do { \
        status_t res = (_expr); \
        if (res < 0) { \
            returnMessage(message); \
            ALOGE("%s: writing message failed: %s (%d)", __FUNCTION__, strerror(-res), res); \
            return; \
        } \
    } while(0)

MessengerToHdrPlusClient::MessengerToHdrPlusClient() : mConnected(false) {
}

MessengerToHdrPlusClient::~MessengerToHdrPlusClient() {
    disconnect();
}

status_t MessengerToHdrPlusClient::connect(EaselMessengerListener &listener) {
    std::lock_guard<std::mutex> lock(mApiLock);

    if (mConnected) return -EEXIST;

    // Connect to HDR+ service.
    status_t res = mEaselCommServer.open(EaselComm::EASEL_SERVICE_HDRPLUS);
    if (res != 0) {
        ALOGE("%s: Opening EaselCommServer failed: %s (%d).", __FUNCTION__, strerror(-res), res);
        return -ENODEV;
    }

    res = EaselMessenger::connect(listener, kMaxHdrPlusMessageSize, &mEaselCommServer);
    if (res != 0) {
        ALOGE("%s: Connecting EaselMessenger failed: %s (%d).", __FUNCTION__, strerror(-res), res);
        mEaselCommServer.close();
        return res;
    }

    mConnected = true;
    return 0;
}

void MessengerToHdrPlusClient::disconnect() {
    std::lock_guard<std::mutex> lock(mApiLock);

    if (!mConnected) return;

    mEaselCommServer.close();
    EaselMessenger::disconnect();
}

void MessengerToHdrPlusClient::notifyFrameEaselTimestampAsync(int64_t easelTimestampNs) {
    std::lock_guard<std::mutex> lock(mApiLock);

    if (!mConnected) {
        ALOGE("%s: Messenger not connected.", __FUNCTION__);
        return;
    }

    // Prepare the message.
    Message *message = nullptr;
    status_t res = getEmptyMessage(&message);
    if (res != 0) {
        ALOGE("%s: Getting empty message failed: %s (%d).", __FUNCTION__, strerror(-res), res);
    }

    RETURN_ON_WRITE_ERROR(message->writeUint32(MESSAGE_NOTIFY_FRAME_EASEL_TIMESTAMP_ASYNC));

    // Serialize timestamp
    RETURN_ON_WRITE_ERROR(message->writeInt64(easelTimestampNs));

    // Send to client.
    res = sendMessage(message, /*async*/true);
    if (res != 0) {
        ALOGE("%s: Sending message failed: %s (%d).", __FUNCTION__, strerror(-res), res);;
    }
}

void MessengerToHdrPlusClient::notifyCaptureResult(CaptureResult *result) {
    std::lock_guard<std::mutex> lock(mApiLock);

    if (!mConnected) {
        ALOGE("%s: Messenger not connected.", __FUNCTION__);
        return;
    }

    // Only one buffer can be transferred via DMA each time so sending a message for every output
    // buffer.
    for (auto buffer : result->outputBuffers) {
        // Prepare the message.
        Message *message = nullptr;
        status_t res = getEmptyMessage(&message);
        if (res != 0) {
            ALOGE("%s: Getting empty message failed: %s (%d).", __FUNCTION__, strerror(-res), res);
        }

        RETURN_ON_WRITE_ERROR(message->writeUint32(MESSAGE_NOTIFY_DMA_CAPTURE_RESULT));

        RETURN_ON_WRITE_ERROR(message->writeUint32(result->requestId));
        RETURN_ON_WRITE_ERROR(message->writeUint32(buffer.streamId));
        RETURN_ON_WRITE_ERROR(message->writeInt64(result->metadata.easelTimestamp));
        RETURN_ON_WRITE_ERROR(message->writeInt64(result->metadata.timestamp));

        // Send to client.
        res = sendMessageWithDmaBuffer(message, buffer.data, buffer.dataSize);
        if (res != 0) {
            ALOGE("%s: Sending message with DMA buffer failed: %s (%d).", __FUNCTION__,
                    strerror(-res), res);
        }
    }
}

void MessengerToHdrPlusClient::notifyShutterAsync(uint32_t requestId, int64_t apSensorTimestampNs) {
    std::lock_guard<std::mutex> lock(mApiLock);

    if (!mConnected) {
        ALOGE("%s: Messenger not connected.", __FUNCTION__);
        return;
    }

    // Prepare the message.
    Message *message = nullptr;
    status_t res = getEmptyMessage(&message);
    if (res != 0) {
        ALOGE("%s: Getting empty message failed: %s (%d).", __FUNCTION__, strerror(-res), res);
    }

    RETURN_ON_WRITE_ERROR(message->writeUint32(MESSAGE_NOTIFY_SHUTTER_ASYNC));

    // Serialize shutter
    RETURN_ON_WRITE_ERROR(message->writeUint32(requestId));
    RETURN_ON_WRITE_ERROR(message->writeInt64(apSensorTimestampNs));

    // Send to client.
    res = sendMessage(message, /*async*/true);
    if (res != 0) {
        ALOGE("%s: Sending message failed: %s (%d).", __FUNCTION__, strerror(-res), res);;
    }
}

} // namespace pbcamera
