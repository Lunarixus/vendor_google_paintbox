#ifndef PAINTBOX_MESSENGER_TO_PB_TI_SERVICE_H
#define PAINTBOX_MESSENGER_TO_PB_TI_SERVICE_H

#include <stdint.h>

#include "PbTiTestRequest.h"
#include "EaselMessenger.h"
#include "easelcomm.h"

namespace pbti {

/*
 * MessengerToPbTiService
 *
 * MessengerToPbTiService class derived from EaselMessenger to send messages to
 * paintbox test service.
 */
class MessengerToPbTiService : public EaselMessenger {
 public:
    MessengerToPbTiService();
    virtual ~MessengerToPbTiService();

    /*
     * Connect to paintbox test service's EaselMessenger.
     *
     * listener is the listener to receive messages from paintbox test service.
     *
     * Returns:
     *  0:          on success.
     *  -EEXIST:    if it's already connected.
     *  -ENODEV:    if connecting failed due to a serious error.
     */
    status_t connect(EaselMessengerListener &listener) override;
    void disconnect() override;

    /*
     * Submit a test request.
     *
     * request is the test request.
     *
     * Returns:
     *  0:              on success.
     *  -EINVAL:        if the request is invalid.
     */
    status_t submitPbTiTestRequest(const PbTiTestRequest &request);

 private:
    // Disconnect with mApiLock held.
    void disconnectWithLockHeld();

    /*
     * Send a message to connect to paintbox test service.
     *
     * Returns:
     *  0:          on success.
     *  -EEXIST:    if it's already connected.
     *  -ENODEV:    if connecting failed due to a serious error.
     */
    status_t connectToService();

    // Send a message to disconnect from paintbox test service.
    status_t disconnectFromService();

    // Protect API methods from being called simultaneously.
    std::mutex mApiLock;

    // If it's currently connected to paintbox test service.
    bool mConnected;

    EaselCommClient mEaselCommClient;
};

}  // namespace pbti

#endif  // PAINTBOX_MESSENGER_TO_PB_TI_SERVICE_H
