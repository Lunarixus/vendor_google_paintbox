#ifndef PAINTBOX_TEST_INFRA_LIBPBTICLIENT_INCLUDE_PBTICLIENT_H
#define PAINTBOX_TEST_INFRA_LIBPBTICLIENT_INCLUDE_PBTICLIENT_H

#include <utils/Errors.h>
#include <utils/Mutex.h>
#include <stdint.h>
#include <deque>

#include "easelcontrol.h"
#include "MessengerToPbTiService.h"
#include "MessengerListenerFromPbTiService.h"
#include "PbTiClientListener.h"

namespace android {

/**
 * PbTiClient
 *
 * PbTiClient class can be used to connect to paintbox test service to perform
 * paintbox test processing on Paintbox.
 */
class PbTiClient : public pbti::MessengerListenerFromPbTiService {
 public:
    PbTiClient();
    virtual ~PbTiClient();

    /*
     * Return if Easel is present on the device.
     *
     * If Easel is not present, all other calls to PbTiClient are invalid.
     */
    bool isEaselPresentOnDevice() const;

    /*
     * Open an easelcontrol connection to Easel.
     *
     * Must be called before other following methods.
     */
    status_t openEasel();

    /*
     * Resume Easel.
     *
     * Resume Easel from suspend mode.
     */
    status_t resumeEasel();

    /*
     * Activate Easel.
     *
     * Put Easel to active mode
     */
    status_t activateEasel();

    /*
     * Deactivate Easel.
     */
    status_t deactivateEasel();

    /*
     * Suspend Easel.
     *
     * Put Easel on suspend mode.
     */
    status_t suspendEasel();

    /*
     * Freeze Easel state.
     *
     */
    status_t freezeEaselState();

    /*
     * Unfreeze Easel state.
     *
     */
    status_t unfreezeEaselState();

    // Close the easelcontrol connection.
    void closeEasel();

    /*
     * Connect to paintbox test service.
     *
     * listener is the listener to receive callbacks from paintbox test client.
     *
     * Returns:
     *  0:          on success.
     *  -EEXIST:    if it's already connected.
     *  -ENODEV:    if connecting failed due to a serious error.
     */
    status_t connect(PbTiClientListener *listener);

    // Disconnect from paintbox test service.
    void disconnect();

    /*
     * Submit a test request for easel.
     *
     * request is a PbTi test request to paintbox test service.
     *
     * Returns:
     *  0:              on success.
     *  -EINVAL:        if the request is invalid.
     */
    status_t submitPbTiTestRequest(const pbti::PbTiTestRequest &request);

 private:
    // Callbacks from paintbox test service start here.
    // Override pbti::MessengerListenerFromPbTiService
    void notifyPbTiTestResult(const std::string &result) override;
    void notifyPbTiTestResultFailed() override;
    // Callbacks from paintbox test service end here.

    // Easel control
    bool mIsEaselPresent;
    EaselControlClient mEaselControl;
    bool mEaselControlOpened;
    Mutex mEaselControlLock;  // Protecting Easel control variables above.

    // EaselMessenger to send messages to paintbox test service.
    pbti::MessengerToPbTiService mMessengerToService;

    // Callbacks to invoke from PbTiClient.
    Mutex mClientListenerLock;
    PbTiClientListener *mClientListener;
};

}  // namespace android

#endif  // PAINTBOX_TEST_INFRA_LIBPBTICLIENT_INCLUDE_PBTICLIENT_H
