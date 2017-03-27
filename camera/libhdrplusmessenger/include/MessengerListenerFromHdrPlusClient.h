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
#ifndef PAINTBOX_MESSENGER_LISTENER_FROM_HDR_PLUS_CLIENT_H
#define PAINTBOX_MESSENGER_LISTENER_FROM_HDR_PLUS_CLIENT_H

#include <stdint.h>

#include "EaselMessenger.h"
#include "HdrPlusMessageTypes.h"
#include "HdrPlusTypes.h"

namespace pbcamera {

class MessengerListenerFromHdrPlusClient : public EaselMessengerListener {
public:
    virtual ~MessengerListenerFromHdrPlusClient() {};

    // The following callback functions must match the ones in MessengerToHdrPlusService.

    /*
     * Invoked when HDR+ client connects to service.
     *
     * Returns:
     *  0:              on success.
     *  -EEXIST:        if it's already connected.
     *  -ENODEV:        if connecting failed due to a serious error.
     */
    virtual status_t connect() = 0;

    /*
     * Invoked when HDR+ client disconnect from service.
     */
    virtual void disconnect() = 0;

    /*
     * Invoked when HDR+ client set static metadata of current camera device.
     *
     * staticMetadata is the static metadata of current camera device.
     *
     * Returns:
     *  0:              on success.
     *  -ENODEV:        if HDR+ service is not connected.
     */
    virtual status_t setStaticMetadata(const StaticMetadata& metadata) = 0;

    /*
     * Invoked when HDR+ client configures streams
     *
     * inputConfig contains the information about the input frames, either from
     * the sensor or the client.
     * outputConfigs is a vector of output stream configurations.
     *
     * Returns:
     *  0:              on success.
     *  -EINVAL:        if inputConfig or outputConfigs contains invalid values
     *                  or configurations that are not supported.
     *  -ENODEV:        if HDR+ service is not connected or it encounters a serious error.
     */
    virtual status_t configureStreams(const InputConfiguration &inputConfig,
            const std::vector<StreamConfiguration> &outputConfigs) = 0;

    /*
     * Enable or disable ZSL HDR+ mode.
     *
     * When ZSL HDR+ mode is enabled, Easel will capture ZSL RAW buffers. ZSL HDR+ mode should be
     * disabled to reduce power consumption when HDR+ processing is not necessary, e.g in video
     * mode.
     *
     * enabled is a flag indicating whether to enable ZSL HDR+ mode.
     *
     * Returns:
     *  0:          on success.
     *  -ENODEV:    if HDR+ service is not connected, or streams are not configured.
     */
    virtual status_t setZslHdrPlusMode(bool enabled) = 0;

    /*
     * Invoked when HDR+ client submits a capture request for HDR+ outputs.
     *
     * request is a CaptureRequest containing request ID and output buffers.
     *
     * Returns:
     *  0:              on success.
     *  -EINVAL:        if the request is invalid such as containing invalid stream IDs.
     */
    virtual status_t submitCaptureRequest(const CaptureRequest &request) = 0;

    /*
     * Invoked when a DMA input buffer is received. If the callback function wants
     * to transfer the DMA buffer to a local buffer, it must call
     * EaselMessenger::transferDmaBuffer() during this callback. After the callback returns,
     * DMA buffer will be discarded.
     *
     * dmaInputBuffer is a DMA input buffer to be transferred.
     * mockingEaselTimestampNs is a mocking Easel timestamp of the input buffer.
     */
    virtual void notifyDmaInputBuffer(const DmaImageBuffer &dmaInputBuffer,
            int64_t mockingEaselTimestampNs) = 0;

    /*
     * Invoked when a frame metadata is received.
     *
     * metadata is the metadata of a frame that AP captured.
     */
    virtual void notifyFrameMetadata(const FrameMetadata &metadata) = 0;

    /*
     * Override EaselMessengerListener::onMessage
     * Invoked when receiving a message from HDR+ client.
     *
     * Returns:
     *  0:          on success.
     *  Non-zero errors depend on the message.
     */
    status_t onMessage(Message *message) override;

    /*
     * Override EaselMessengerListener::onMessageWithDmaBuffer
     * Invoked when receiving a message with a DMA buffer from HDR+ client.
     *
     * Returns:
     *  0:          on success.
     *  Non-zero errors depend on the message.
     */
    status_t onMessageWithDmaBuffer(Message *message, DmaBufferHandle handle,
            uint32_t dmaDataSize) override;

private:
    /*
     * Functions to deserialize messages.
     *
     * Returns:
     *  0:          on success.
     *  -ENODATA:   if deserialing the message failed.
     *  Non-zero errors also depends on each message.
     */
    status_t deserializeConnect(Message *message);
    status_t deserializeSetStaticMetadata(Message *message);
    status_t deserializeConfigureStreams(Message *message);
    status_t deserializeSetZslHdrPlusMode(Message *message);
    status_t deserializeSubmitCaptureRequest(Message *message);
    void deserializeNotifyDmaInputBuffer(Message *message, DmaBufferHandle dmaHandle,
            int dmaDataSize);
    void deserializeNotifyFrameMetadata(Message *message);

    // Read a stream configuration from a message.
    status_t readStreamConfiguration(Message *message, StreamConfiguration *config);
};

} // namespace pbcamera

#endif // PAINTBOX_MESSENGER_LISTENER_FROM_HDR_PLUS_CLIENT_H
