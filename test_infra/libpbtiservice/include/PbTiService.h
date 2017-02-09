#ifndef PAINTBOX_TEST_INFRA_LIBPBTISERVICE_INCLUDE_PBTISERVICE_H
#define PAINTBOX_TEST_INFRA_LIBPBTISERVICE_INCLUDE_PBTISERVICE_H

#include <mutex>

#include "easelcontrol.h"
#include "MessengerToPbTiClient.h"
#include "MessengerListenerFromPbTiClient.h"

namespace pbti {

/**
 * PbTiService
 *
 * PbTiService class is a service that listens to messages from PbTiClient and
 * performs requested processing.
 */
class PbTiService : public MessengerListenerFromPbTiClient {
 public:
    PbTiService();
    virtual ~PbTiService();

    /*
     * Start service.
     * Returns:
     *  0:          if the service starts successfully.
     *  -EEXIST:    if the service is already started.
     *  -ENODEV:    if the service cannot be started due to a serious error.
     */
    status_t start();

    /*
     * Stop service.
     * Returns:
     *  0:          if the service stops successfully.
     *  -ENODEV:    if the service cannot be stopped due to a serious error.
     */
    status_t stop();

    /*
     * Wait for the service to finish.
     * PbTi service should be alive at all time, so this function will not
     * return during normal operations.
     */
    void wait();

 private:
    // Callbacks from PbTi client start here.
    status_t connect() override;
    void disconnect() override;
    status_t submitPbTiTestRequest(const PbTiTestRequest &request) override;
    // Callbacks from PbTi client end here.

    // Stop the service with mApiLock held.
    void stopLocked();

    // Protect API methods from being called simultaneously.
    std::mutex mApiLock;

    std::condition_variable mExitCondition;

    // Easel control.
    EaselControlServer mEaselControl;

    // MessengerToPbTiClient to send messages to paintbox test client.
    MessengerToPbTiClient mMessengerToClient;
};

}  // namespace pbti

#endif  // PAINTBOX_TEST_INFRA_LIBPBTISERVICE_INCLUDE_PBTISERVICE_H
