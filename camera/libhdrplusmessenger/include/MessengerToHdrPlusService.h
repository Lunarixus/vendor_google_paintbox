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
#ifndef PAINTBOX_MESSENGER_TO_HDR_PLUS_SERVICE_H
#define PAINTBOX_MESSENGER_TO_HDR_PLUS_SERVICE_H

#include <stdint.h>

#include "EaselMessenger.h"
#include "HdrPlusTypes.h"
#include "easelcomm.h"

namespace pbcamera {

/*
 * MessengerToHdrPlusService
 *
 * MessengerToHdrPlusService class derived from EaselMessenger to send messages to HDR+ service.
 */
class MessengerToHdrPlusService : public EaselMessenger {
public:
    MessengerToHdrPlusService();
    virtual ~MessengerToHdrPlusService();

    /*
     * Connect to HDR+ service's EaselMessenger.
     *
     * listener is the listener to receive messages from HDR+ service.
     *
     * Returns:
     *  0:          on success.
     *  -EEXIST:    if it's already connected.
     *  -ENODEV:    if connecting failed due to a serious error.
     */
    status_t connect(EaselMessengerListener &listener) override;

    /*
     * Disconnect from HDR+ service.
     *
     * isErrorState indicate if HDR+ service or Easel is in an error state. If true, it won't send
     *              any messsage to Easel because it may hang.
     */
    void disconnect(bool isErrorState = false) override;

    /*
     * The following functions will invoke sending messages to HDR+ service. These should match
     * the ones in MessengerListenerFromHdrPlusClient.
     */

    /*
     * Set the static metadata of current camera device.
     *
     * metadata is the static metadata.
     *
     * Returns:
     *  0:         on success.
     *  -ENODEV:   if HDR+ service is not connected.
     */
     status_t setStaticMetadata(const StaticMetadata& metadata);

    /*
     * Configure streams
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
    status_t configureStreams(const pbcamera::InputConfiguration &inputConfig,
            const std::vector<StreamConfiguration> &outputConfigs);

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
    status_t setZslHdrPlusMode(bool enabled);

    /*
     * Submit a capture request. For each capture request,
     * MessengerListenerFromHdrPlusService::dmaCaptureResult will be invoked one or more times to
     * return all or a subset of output buffers.
     *
     * request is the capture request. Each capture request must have a unique CaptureRequest.id.
     * metadata is the request metadata.
     *
     * Returns:
     *  0:              on success.
     *  -EINVAL:        if the request is invalid such as containing invalid stream IDs.
     */
    status_t submitCaptureRequest(CaptureRequest *request, const RequestMetadata &metadata);

    /*
     * Send an input buffer to HDR+ service. This is used when HDR+ service's input buffers come
     * from the client rather than MIPI.
     *
     * inputBuffer is the input buffer to send to HDR+ service. After this method returns, the
     *             buffer has been copied (with DMA) to HDR+ service and the caller has the
     *             ownership of the buffer.
     * mockingEaselTimestampNs is a mocking time at start of exposure of first row of the image
     *                         sensor active array, in nanoseconds. This will be used as the Easel
     *                         timestamp of the input buffer.
     */
    void notifyInputBuffer(const StreamBuffer &inputBuffer, int64_t mockingEaselTimestampNs);

    /*
     * Send a frame metadata to HDR+ service asynchronously. This may return before HDR+ service
     * receives the frame metadata.
     *
     * metadata is the metadata that will be copied and sent to HDR+ service.
     */
    void notifyFrameMetadataAsync(const FrameMetadata &metadata);

private:
    // Disconnect with mApiLock held.
    void disconnectLocked(bool isErrorState);

    /*
     * Send a message to connect to HDR+ service.
     *
     * Returns:
     *  0:          on success.
     *  -EEXIST:    if it's already connected.
     *  -ENODEV:    if connecting failed due to a serious error.
     */
    status_t connectToService();

    // Send a message to disconnect from HDR+ service.
    status_t disconnectFromService(bool isErrorState);

    // Write a stream configuration to a message.
    status_t writeStreamConfiguration(Message *message, const StreamConfiguration &config);

    // Protect API methods from being called simultaneously.
    std::mutex mApiLock;

    // If it's currently connected to HDR+ service.
    bool mConnected;

    EaselCommClient mEaselCommClient;
};

} // namespace pbcamera

#endif // PAINTBOX_MESSENGER_TO_HDR_PLUS_SERVICE_H
