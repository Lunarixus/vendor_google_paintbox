#define LOG_TAG "EaselManagerClientImpl"
#include <log/log.h>

#define ENABLE_HDRPLUS_PROFILER 1
#include "HdrPlusProfiler.h"

#include "EaselManagerClientImpl.h"
#include "HdrPlusClientImpl.h"

namespace android {

EaselManagerClientImpl::EaselManagerClientImpl() : mEaselControlOpened(false),
                                                   mEaselResumed(false),
                                                   mEaselActivated(false),
                                                   mEaselManagerClientListener(nullptr) {
    mIsEaselPresent = ::isEaselPresent();
    ALOGI("%s: Easel is %s", __FUNCTION__, mIsEaselPresent ? "present" : "not present");
}

EaselManagerClientImpl::~EaselManagerClientImpl() {
    Mutex::Autolock l(mEaselControlLock);
    deactivateLocked();
    suspendLocked();
}

bool EaselManagerClientImpl::isEaselPresentOnDevice() const {
    return mIsEaselPresent;
}

status_t EaselManagerClientImpl::open() {
    Mutex::Autolock l(mEaselControlLock);
    if (mEaselControlOpened) {
        ALOGW("%s: Easel control is already opened.", __FUNCTION__);
        return OK;
    }

    status_t res;

#if !USE_LIB_EASEL
    // Open Easel control.
    res = mEaselControl.open(mDefaultServerHost);
#else
    SCOPE_PROFILER_TIMER("Open EaselControl");
    res = mEaselControl.open();
#endif
    if (res != OK) {
        ALOGE("%s: Failed to open Easel control: %s (%d).", __FUNCTION__, strerror(errno), -errno);
        return NO_INIT;
    }

    mEaselControl.registerErrorCallback(std::bind(&EaselManagerClientImpl::onEaselError,
            this, std::placeholders::_1, std::placeholders::_2));
    mEaselControlOpened = true;
    mEaselResumed = false;
    return res;
}

int EaselManagerClientImpl::onEaselError(enum EaselErrorReason r, enum EaselErrorSeverity s) {

    std::string errMsg;
    switch (s) {
        case EaselErrorSeverity::FATAL:
            errMsg.append("Fatal: ");
            break;
        case EaselErrorSeverity::NON_FATAL:
            errMsg.append("Non-fatal: ");
            break;
        default:
            break;
    }

    switch (r) {
        case EaselErrorReason::LINK_FAIL:
            errMsg.append("PCIe link down.");
            break;
        case EaselErrorReason::BOOTSTRAP_FAIL:
            errMsg.append("AP didn't receive bootstrap msi.");
            break;
        case EaselErrorReason::OPEN_SYSCTRL_FAIL:
            errMsg.append("AP failed to open SYSCTRL service.");
            break;
        case EaselErrorReason::HANDSHAKE_FAIL:
            errMsg.append("Handshake failed.");
            break;
        case EaselErrorReason::IPU_RESET_REQ:
            errMsg.append("Easel requested AP to reset it.");
            break;
        default:
            errMsg.append("Unknown error.");
            break;
    }

    ALOGE("%s: Got an Easel error: %s (%d).", __FUNCTION__, errMsg.c_str(), r);

    Mutex::Autolock l(mClientListenerLock);
    if (mEaselManagerClientListener == nullptr) {
        ALOGE("%s: Listener is not set.", __FUNCTION__);
        return NO_INIT;
    }

    if (s != EaselErrorSeverity::FATAL) {
        ALOGI("%s: Ignore non-fatal Easel error", __FUNCTION__);
        return 0;
    }

    mEaselManagerClientListener->onEaselFatalError(errMsg);
    return 0;
}

status_t EaselManagerClientImpl::suspend() {
    Mutex::Autolock l(mEaselControlLock);
    return suspendLocked();
}

bool EaselManagerClientImpl::isOpenFuturePendingLocked() {
    if (!mOpenFuture.valid()) {
        return false;
    }

    // If open future is valid, check if it is still pending.
    return mOpenFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready;
}

status_t EaselManagerClientImpl::suspendLocked() {
    ALOGD("%s: Suspending Easel.", __FUNCTION__);
    if (!mEaselControlOpened) {
        ALOGE("%s: Easel control is not opened.", __FUNCTION__);
        return NO_INIT;
    }

    if (isOpenFuturePendingLocked()) {
        if (mOpenFuture.wait_for(std::chrono::milliseconds(kHdrPlusClientOpeningTimeoutMs)) !=
                std::future_status::ready) {
            ALOGW("%s: Waiting for opening HDR+ client to complete timed out after %u ms. "
                    "Continue suspending.", __FUNCTION__, kHdrPlusClientOpeningTimeoutMs);
        }
    }

    deactivateLocked();

    SCOPE_PROFILER_TIMER("Suspend Easel");
    status_t res = mEaselControl.suspend();

    mEaselResumed = false;
    return res;
}

status_t EaselManagerClientImpl::resume(EaselManagerClientListener *listener) {
    ALOGD("%s: Resuming Easel.", __FUNCTION__);
    Mutex::Autolock l(mEaselControlLock);
    if (!mEaselControlOpened) {
        ALOGE("%s: Easel control is not opened.", __FUNCTION__);
        return NO_INIT;
    }

    if (mEaselResumed) {
        ALOGD("%s: Easel is already resumed.", __FUNCTION__);
        return -EUSERS;
    }

    {
        Mutex::Autolock l(mClientListenerLock);
        mEaselManagerClientListener = listener;
    }

    SCOPE_PROFILER_TIMER("Resume Easel");
    status_t res = mEaselControl.resume();
    if (res) {
        ALOGE("%s: Resume Easel failed: %s (%d)", __FUNCTION__, strerror(-res), res);
        return res;
    }

    mEaselResumed = true;
    return OK;
}

status_t EaselManagerClientImpl::convertCameraId(uint32_t cameraId,
        enum EaselControlClient::Camera *easelCameraId) {
    if (easelCameraId == nullptr) return BAD_VALUE;

    switch (cameraId) {
        case 0:
            *easelCameraId = EaselControlClient::MAIN;
            break;
        case 1:
            *easelCameraId = EaselControlClient::FRONT;
            break;
        default:
            ALOGE("%s: camera ID %u not supported." , __FUNCTION__, cameraId);
            return BAD_VALUE;
    }
    return OK;
}

status_t EaselManagerClientImpl::startMipi(uint32_t cameraId, uint32_t outputPixelClkHz,
        bool enableCapture) {
    Mutex::Autolock l(mEaselControlLock);
    if (!mEaselControlOpened) {
        ALOGE("%s: Easel control is not opened.", __FUNCTION__);
        return NO_INIT;
    }

    uint32_t rate = outputPixelClkHz * kApEaselMipiRateConversion;
    enum EaselControlClient::Camera easelCameraId;

    status_t res = convertCameraId(cameraId, &easelCameraId);
    if (res != OK) {
        ALOGE("%s: Converting camera id failed: %s (%d)", __FUNCTION__, strerror(-res), res);
        return res;
    }

    ALOGD("%s: Start MIPI rate %d for camera %u enableCapture %d", __FUNCTION__, rate,
            cameraId, enableCapture);

    SCOPE_PROFILER_TIMER("Start MIPI");
    res = mEaselControl.startMipi(easelCameraId, rate, enableCapture);
    if (res != OK) {
        ALOGE("%s: Failed to config mipi: %s (%d).", __FUNCTION__, strerror(errno), -errno);
        return NO_INIT;
    }

    return res;
}

status_t EaselManagerClientImpl::stopMipi(uint32_t cameraId) {
    Mutex::Autolock l(mEaselControlLock);
    if (!mEaselControlOpened) {
        ALOGE("%s: Easel control is not opened.", __FUNCTION__);
        return NO_INIT;
    }

    enum EaselControlClient::Camera easelCameraId;

    status_t res = convertCameraId(cameraId, &easelCameraId);
    if (res != OK) {
        ALOGE("%s: Converting camera id failed: %s (%d)", __FUNCTION__, strerror(-res), res);
        return res;
    }

    ALOGD("%s: Stop MIPI for camera %u", __FUNCTION__, cameraId);

    SCOPE_PROFILER_TIMER("Stop MIPI");
    res = mEaselControl.stopMipi(easelCameraId);
    if (res != OK) {
        ALOGE("%s: Failed to config mipi: %s (%d).", __FUNCTION__, strerror(errno), -errno);
        return NO_INIT;
    }

    return res;
}

status_t EaselManagerClientImpl::openHdrPlusClientInternal(HdrPlusClientListener *listener,
        std::unique_ptr<HdrPlusClient> *client) {
    // If client is valid, this function is called synchronously.
    bool isCalledSynchronously = client != nullptr;

    Mutex::Autolock l(mEaselControlLock);

    // Activate Easel.
    status_t res = activateLocked();
    if (res != OK) {
        ALOGE("%s: Activating Easel failed: %s (%d)", __FUNCTION__, strerror(-res), res);

        if (!isCalledSynchronously) {
            listener->onOpenFailed(res);
        }

        return res;
    }

    // Create a new HDR+ client.
    auto newClient = std::make_unique<HdrPlusClientImpl>();

    // Connect to the messenger for sending messages to HDR+ service.
    res = newClient->connect(listener);
    if (res != OK) {
        ALOGE("%s: Connecting service messenger failed: %s (%d)", __FUNCTION__,
                strerror(-res), res);

        if (!isCalledSynchronously) {
            listener->onOpenFailed(res);
        }

        return res;
    }

    if (!isCalledSynchronously) {
        listener->onOpened(std::move(newClient));
    } else {
        *client = std::move(newClient);
    }

    return OK;
}

status_t EaselManagerClientImpl::openHdrPlusClientAsync(HdrPlusClientListener *listener) {
    Mutex::Autolock l(mEaselControlLock);
    if (isOpenFuturePendingLocked()) {
        ALOGE("%s: HDR+ client is already being opened.", __FUNCTION__);
        return ALREADY_EXISTS;
    }

    // Launch a future to open an HDR+ client.
    mOpenFuture = std::async(std::launch::async, &EaselManagerClientImpl::openHdrPlusClientInternal,
            this, listener, /* client */nullptr);

    return OK;
}

status_t EaselManagerClientImpl::openHdrPlusClient(HdrPlusClientListener *listener,
        std::unique_ptr<HdrPlusClient> *client) {
    return openHdrPlusClientInternal(listener, client);
}

void EaselManagerClientImpl::closeHdrPlusClient(std::unique_ptr<HdrPlusClient> client) {
    client = nullptr;

    Mutex::Autolock l(mEaselControlLock);
    status_t res = deactivateLocked();
    if (res != OK) {
        ALOGE("%s: Deactivating Easel failed: %s (%d)", __FUNCTION__, strerror(-res), res);
    }
}

status_t EaselManagerClientImpl::activateLocked() {
    if (!mEaselControlOpened) {
        ALOGE("%s: Easel control is not opened.", __FUNCTION__);
        return NO_INIT;
    }

    if (mEaselActivated) {
        ALOGE("%s: Easel is already activated.", __FUNCTION__);
        return ALREADY_EXISTS;
    }

    SCOPE_PROFILER_TIMER("Activate Easel");

    // Activate Easel.
    status_t res = mEaselControl.activate();
    if (res != OK) {
        ALOGE("%s: Failed to activate Easel: %s (%d).", __FUNCTION__, strerror(errno), -errno);
        return NO_INIT;
    }
    mEaselActivated = true;
    return OK;
}

status_t EaselManagerClientImpl::deactivateLocked() {
    if (!mEaselActivated) return OK;

    SCOPE_PROFILER_TIMER("Deactivate Easel");
    status_t res = mEaselControl.deactivate();
    if (res != OK) {
        ALOGE("%s: Failed to activate Easel: %s (%d).", __FUNCTION__, strerror(errno), -errno);
        return res;
    }
    mEaselActivated = false;
    return OK;
}

} // namespace android
