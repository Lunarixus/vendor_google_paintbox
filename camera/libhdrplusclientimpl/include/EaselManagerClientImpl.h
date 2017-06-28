#ifndef PAINTBOX_EASEL_MANAGER_CLIENT_IMPL_H
#define PAINTBOX_EASEL_MANAGER_CLIENT_IMPL_H

#include <future>

#include <utils/Errors.h>
#include <utils/Mutex.h>

#include "EaselManagerClient.h"
#include "easelcontrol.h"

namespace android {

class HdrPlusClient;
class HdrPlusClientListener;

class EaselManagerClientImpl : public EaselManagerClient {
public:
    EaselManagerClientImpl();
    virtual ~EaselManagerClientImpl();

    /*
     * Return if Easel is present on the device.
     *
     * If Easel is not present, all other calls to HdrPlusClient are invalid.
     */
    bool isEaselPresentOnDevice() const override;

    /*
     * Open Easel manager client.
     *
     * This will power on Easel and initialize Easel manager client.
     */
    status_t open() override;

    /*
     * Suspend Easel.
     *
     * Put Easel on suspend mode.
     */
    status_t suspend() override;

    /*
     * Resume Easel.
     *
     * Resume Easel from suspend mode.
     */
    status_t resume() override;

    /*
     * Start MIPI with an output pixel lock rate for a camera.
     *
     * Can be called when Easel is powered on or resumed, for Easel to start sending sensor data
     * to AP.
     *
     * cameraId is the camera ID to start MIPI for.
     * outputPixelClkHz is the output pixel rate.
     * enableCapture is whether to enable MIPI capture on Easel.
     */
    status_t startMipi(uint32_t cameraId, uint32_t outputPixelClkHz,
                       bool enableCapture) override;

    /*
     * Stop MIPI for a camera.
     *
     * cameraId is the camera is ID to stop MIPI for.
     */
    status_t stopMipi(uint32_t cameraId) override;

    /*
     * Open an HDR+ client asynchronously.
     *
     * Open an HDR+ client asynchronouly. When an HDR+ client is opened,
     * HdrPlusClientListener::onOpened() will be invoked with the created HDR+ client. If opening
     * an HDR+ client failed, HdrPlusClientListener::onOpenFailed() will be invoked with the error.
     * If this method returns an error, HdrPlusClientListener::onOpenFailed() will not be invoked.
     *
     * listener is an HDR+ client listener.
     *
     * Returns:
     *  OK:             if initiating opening an HDR+ client asynchronously was successful.
     *                  HdrPlusClientListener::onOpened() or HdrPlusClientListener::onOpenFailed()
     *                  will be invoked when opening an HDR+ client completed.
     *  ALREADY_EXISTS: if there is already a pending HDR+ client being opened.
     */
    status_t openHdrPlusClientAsync(HdrPlusClientListener *listener) override;

    /*
     * Open an HDR+ client synchronously and block until it completes.
     *
     * listener is an HDR+ client listener.
     * client is an output parameter for created HDR+ client.
     *
     * Returns:
     *  OK:         on success.
     *  -EEXIST:    if an HDR+ client is already opened.
     *  -ENODEV:    if opening an HDR+ failed due to a serious error.
     */
    status_t openHdrPlusClient(HdrPlusClientListener *listener,
            std::unique_ptr<HdrPlusClient> *client) override;

    /*
     * Close an HDR+ client.
     *
     * client is the HDR+ client to be closed.
     */
    void closeHdrPlusClient(std::unique_ptr<HdrPlusClient> client) override;

private:

    // TODO: This should be caculated from the number of lanes and data bits. Should fix this
    // after those are available in HAL.
    static constexpr float kApEaselMipiRateConversion = 0.0000025f;

    // Time to wait for HDR+ client opening to complete.
    const uint32_t kHdrPlusClientOpeningTimeoutMs = 5000; // 5 seconds.

#if !USE_LIB_EASEL
    // Used to connect Easel control. Only needed for TCP/IP mock.
    const char *mDefaultServerHost = "localhost";
#endif

    // If there is a pending open of HDR+ client. Must be called with mEaselControlLock held.
    bool isOpenFuturePendingLocked();

    // Open an HDR+ client. If client is nullptr, this is called synchronously. Otherwise, this is
    // called asynchronously in a future.
    status_t openHdrPlusClientInternal(HdrPlusClientListener *listener,
            std::unique_ptr<HdrPlusClient> *client);

    // Activate Easel. Must be called with mEaselControlLock held.
    status_t activateLocked();

    // Deactivate Easel. Must be called with mEaselControlLock held.
    status_t deactivateLocked();

    // Suspend Easel. Must be called with mEaselControlLock held.
    status_t suspendLocked();

    // Convert HAL camera ID to Easel control client enum.
    status_t convertCameraId(uint32_t cameraId, enum EaselControlClient::Camera *easelCameraId);

    // If Easel is present on the device.
    bool mIsEaselPresent;

    // Easel control client. Protected by mEaselControlLock.
    EaselControlClient mEaselControl;

    // If Easel control client is opened. Protected by mEaselControlLock.
    bool mEaselControlOpened;

    // If Easel is resumed. Protected by mEaselControlLock.
    bool mEaselResumed;

    // If Easel is activated. Protected by mEaselControlLock.
    bool mEaselActivated;

    // A future of opening an HDR+ client. Protected by mEaselControlLock.
    std::future<status_t> mOpenFuture;

    Mutex mEaselControlLock;
};

} // namespace android

#endif // PAINTBOX_EASEL_MANAGER_CLIENT_IMPL_H
