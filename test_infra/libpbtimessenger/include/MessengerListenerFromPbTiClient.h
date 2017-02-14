#ifndef PAINTBOX_MESSENGER_LISTENER_FROM_PB_TI_CLIENT_H
#define PAINTBOX_MESSENGER_LISTENER_FROM_PB_TI_CLIENT_H

#include <stdint.h>

#include "PbTiTestRequest.h"
#include "EaselMessenger.h"
#include "PbTiMessageTypes.h"

namespace pbti {

class MessengerListenerFromPbTiClient : public EaselMessengerListener {
 public:
    virtual ~MessengerListenerFromPbTiClient() {}

    // The following callback functions must match the ones in
    // MessengerToPbTiService.

    /*
     * Invoked when paintbox test client connects to service.
     *
     * Returns:
     *  0:              on success.
     *  -EEXIST:        if it's already connected.
     *  -ENODEV:        if connecting failed due to a serious error.
     */
    virtual status_t connect() = 0;

    /*
     * Invoked when paintbox test client disconnect from service.
     */
    virtual void disconnect() = 0;

    /*
     * Invoked when paintbox test client submits a test request.
     *
     * request is a test request.
     *
     * Returns:
     *  0:              on success.
     *  -EINVAL:        if the request is invalid.
     */
    virtual status_t submitPbTiTestRequest(const PbTiTestRequest &request) = 0;

    /*
     * Override EaselMessengerListener::onMessage
     * Invoked when receiving a message from paintbox test client.
     *
     * Returns:
     *  0:          on success.
     *  Non-zero errors depend on the message.
     */
    status_t onMessage(Message *message) override;

 private:
    /*
     * Functions to deserialize messages.
     *
     * Returns:
     *  0:          on success.
     *  -ENODATA:   if deserialing the message failed.
     *  Non-zero errors also depends on each message.
     */
    status_t deserializeSubmitPbTiTestRequest(Message *message);
};

}  // namespace pbti

#endif  // PAINTBOX_MESSENGER_LISTENER_FROM_PB_TI_CLIENT_H
