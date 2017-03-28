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
#ifndef PAINTBOX_MESSENGER_TO_HDR_PLUS_CLIENT_H
#define PAINTBOX_MESSENGER_TO_HDR_PLUS_CLIENT_H

#include "EaselMessenger.h"
#include "mockeaselcomm.h"
#include "HdrPlusMessageTypes.h"
#include "HdrPlusTypes.h"

namespace pbcamera {

/*
 * MessengerToHdrPlusClient
 *
 * MessengerToHdrPlusClient class derived from EaselMessenger to send messages to HDR+ client.
 */
class MessengerToHdrPlusClient : public EaselMessenger {
public:
    MessengerToHdrPlusClient();
    virtual ~MessengerToHdrPlusClient();

    /*
     * Connect to HDR+ service's EaselMessenger.
     *
     * listener is the listener to receive messages from HDR+ service.
     * maxMessageSize is the maximal size in bytes of a message.
     *
     * Returns:
     *  0:          on success.
     *  -EEXIST:    if it's already connected.
     *  -ENODEV:    if connecting failed due to a serious error.
     */
    status_t connect(EaselMessengerListener &listener) override;

    // Disconnect from HDR+ service.
    void disconnect() override;

    /*
     * The following functions will invoke messages sent to HDR+ client. These should match
     * the ones in MessengerToHdrPlusClientListener.
     */

    /*
     * Send a frame timestamp to HDR+ client.
     */
    void notifyFrameEaselTimestampAsync(int64_t easelTimestampNs);

    /*
     * Send a capture result to HDR+ client.
     */
    void notifyCaptureResult(CaptureResult *result);
private:

    // Protect API methods from being called simultaneously.
    std::mutex mApiLock;

    // If it's currently connected to HDR+ client.
    bool mConnected;

#if USE_LIB_EASEL
    EaselCommServer mEaselCommServer;
#else
    EaselCommServerNet mEaselCommServer;
#endif

};

} // namespace pbcamera

#endif // PAINTBOX_MESSENGER_TO_HDR_PLUS_CLIENT_H