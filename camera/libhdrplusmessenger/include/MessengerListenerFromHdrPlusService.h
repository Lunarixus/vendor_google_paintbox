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
#ifndef PAINTBOX_HDR_PLUS_SERVICE_MESSENGER_LISTENER_H
#define PAINTBOX_HDR_PLUS_SERVICE_MESSENGER_LISTENER_H

#include <stdint.h>

#include "EaselMessenger.h"
#include "HdrPlusMessageTypes.h"

namespace pbcamera {

/*
 * MessengerListenerFromHdrPlusService
 *
 * An Easel messenger listener class to receive callbacks from HDR+ service.
 */
class MessengerListenerFromHdrPlusService : public EaselMessengerListener {
public:
    virtual ~MessengerListenerFromHdrPlusService() {};

    // The following callbacks should match the ones in MessengerToHdrPlusClient.

    // Invoked when a frame was captured with a framestamp.
    virtual void notifyFrameEaselTimestamp(int64_t easelTimestampNs) = 0;

    /*
     * Invoked when a capture result with a DMA buffer is received. If the callback function wants
     * to transfer the DMA buffer to a local buffer, it must call
     * EaselMessenger::transferDmaBuffer() during this callback. After the callback returns,
     * DMA buffer will be discarded.
     */
    virtual void notifyDmaCaptureResult(DmaCaptureResult *result) = 0;

    /*
     * Override EaselMessengerListener::onMessage
     * Invoked when receiving a message from HDR+ service.
     *
     * Returns:
     *  0:          on success.
     *  Non-zero errors depend on the message.
     */
    status_t onMessage(Message *message) override;

    /*
     * Override EaselMessengerListener::onMessageWithDmaBuffer
     * Invoked when receiving a message with a DMA buffer from HDR+ service.
     *
     * Returns:
     *  0:          on success.
     *  Non-zero errors depend on the message.
     */
    status_t onMessageWithDmaBuffer(Message *message, DmaBufferHandle handle,
            uint32_t dmaDataSize) override;

private:
    // Functions to deserialize messages.
    void deserializeNotifyFrameEaselTimestamp(Message *message);
    void deserializeNotifyDmaCaptureResult(Message *message, DmaBufferHandle handle,
            int dmaDataSize);
};

} // namespace pbcamera

#endif // PAINTBOX_HDR_PLUS_SERVICE_MESSENGER_LISTENER_H
