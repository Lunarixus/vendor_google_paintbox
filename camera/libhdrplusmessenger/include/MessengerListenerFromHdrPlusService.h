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
     * Invoked when HDR+ service has been closed, which may happens when the sevice closes
     * EaselComm or the service has crashed.
     */
    virtual void notifyServiceClosed() = 0;

    /*
     * Invoked when HDR+ processing has started for a request. requestId is the ID of the request.
     * apSensorTimestampNs is the AP sensor timestamp of the base frame, in nanoseconds.
     */
    virtual void notifyShutter(uint32_t requestId, int64_t apSensorTimestampNs) = 0;

    /*
     * Invoked when a makernote was generated for a capture request.
     */
    virtual void notifyDmaMakernote(DmaMakernote *dmaMakernote) = 0;

    /*
     * Invoked when a postview for a request is available.
     */
    virtual void notifyDmaPostview(uint32_t requestId, void *dmaHandle, uint32_t width,
            uint32_t height, uint32_t stride, int32_t format) = 0;
    /*
     * Invoked when HDR+ service requests to dump data to a file via DMA transfer.
     */
    virtual void notifyDmaFileDump(const std::string &filename, DmaBufferHandle dmaHandle,
            uint32_t dmaDataSize) = 0;

private:
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

    /*
     * Override EaselMessengerListener::onEaselCommClosed.
     * Invoked when EaselComm is closed.
     */
    void onEaselCommClosed() override;

    // Functions to deserialize messages.
    void deserializeNotifyFrameEaselTimestamp(Message *message);
    void deserializeNotifyDmaCaptureResult(Message *message, DmaBufferHandle handle,
            int dmaDataSize);
    void deserializeNotifyShutter(Message *message);
    void deserializeNotifyDmaMakernote(Message *message, DmaBufferHandle handle,
            int dmaDataSize);
    void deserializeNotifyDmaPostview(Message *message, DmaBufferHandle handle,
            int dmaDataSize);
    void deserializeNotifyDmaFileDump(Message *message, DmaBufferHandle handle,
            int dmaDataSize);
};

} // namespace pbcamera

#endif // PAINTBOX_HDR_PLUS_SERVICE_MESSENGER_LISTENER_H
