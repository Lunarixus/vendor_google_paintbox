#ifndef PAINTBOX_MESSENGER_LISTENER_FROM_PB_TI_SERVICE_H
#define PAINTBOX_MESSENGER_LISTENER_FROM_PB_TI_SERVICE_H

#include <stdint.h>

#include "EaselMessenger.h"
#include "PbTiMessageTypes.h"

namespace pbti {

/*
 * MessengerListenerFromPbTiService
 *
 * An Easel messenger listener class to receive callbacks from paintbox test
 * service.
 */
class MessengerListenerFromPbTiService : public EaselMessengerListener {
 public:
    virtual ~MessengerListenerFromPbTiService() {}

    // The following callbacks should match the ones in MessengerToPbTiClient.

    /*
     * Invoked when a test result is received.
     */
    virtual void notifyPbTiTestResult(const std::string &result) = 0;

    /*
     * Invoked when a test result is not received.
     */
    virtual void notifyPbTiTestResultFailed() = 0;

    /*
     * Override EaselMessengerListener::onMessage
     * Invoked when receiving a message from paintbox test service.
     *
     * Returns:
     *  0:          on success.
     *  Non-zero errors depend on the message.
     */
    status_t onMessage(Message *message) override;

    /*
     * Invoked when a message is not received from test service.
     */
    void onMessageFailed();

 private:
    // Functions to deserialize messages.
    void deserializeNotifyTestResult(Message *message);
};

}  // namespace pbti

#endif  // PAINTBOX_MESSENGER_LISTENER_FROM_PB_TI_SERVICE_H
