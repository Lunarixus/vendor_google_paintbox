#ifndef PAINTBOX_MESSENGER_TO_PB_TI_CLIENT_H
#define PAINTBOX_MESSENGER_TO_PB_TI_CLIENT_H

#include "EaselMessenger.h"
#include "mockeaselcomm.h"
#include "PbTiMessageTypes.h"

namespace pbti {

/*
 * MessengerToPbTiClient
 *
 * MessengerToPbTiClient class derived from EaselMessenger to send messages to
 * paintbox test client.
 */
class MessengerToPbTiClient : public EaselMessenger {
 public:
    MessengerToPbTiClient();
    virtual ~MessengerToPbTiClient();

    /*
     * Connects to paintbox test service's EaselMessenger.
     *
     * listener is the listener to receive messages from paintbox test service.
     *
     * Returns:
     *  0:          on success.
     *  -EEXIST:    if it's already connected.
     *  -ENODEV:    if connecting failed due to a serious error.
     */
    status_t connect(EaselMessengerListener &listener) override;

    // Disconnects from paintbox test service.
    void disconnect() override;

    /*
     * The following functions will invoke messages sent to paintbox test
     * client. These should match the ones in MessengerListenerFromPbTiService.
     */

     /*
      * Sends a test result to paintbox test client.
      */
     void notifyPbTiTestResult(const std::string &result);

 private:
    // Protect API methods from being called simultaneously.
    std::mutex mApiLock;

    // If it's currently connected to paintbox test client.
    bool mConnected;

#if USE_LIB_EASEL
    EaselCommServer mEaselCommServer;
#else
    EaselCommServerNet mEaselCommServer;
#endif
};

}  // namespace pbti

#endif  // PAINTBOX_MESSENGER_TO_PB_TI_CLIENT_H
