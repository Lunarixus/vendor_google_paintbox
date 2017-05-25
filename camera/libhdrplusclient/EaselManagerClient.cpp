#define LOG_TAG "EaselManagerClient"
#include <utils/Log.h>

#define ENABLE_HDRPLUS_PROFILER 1
#include "HdrPlusProfiler.h"

#include "EaselManagerClient.h"
#include "HdrPlusClient.h"

namespace android {

EaselManagerClient::EaselManagerClient() : mEaselControlOpened(false), mEaselActivated(false) {
    mIsEaselPresent = ::isEaselPresent();
    ALOGI("%s: Easel is %s", __FUNCTION__, mIsEaselPresent ? "present" : "not present");
}

EaselManagerClient::~EaselManagerClient() {
    Mutex::Autolock l(mEaselControlLock);
    deactivateLocked();
    suspendLocked();
}

bool EaselManagerClient::isEaselPresentOnDevice() const {
    return mIsEaselPresent;
}

status_t EaselManagerClient::open() {
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

    mEaselControlOpened = true;
    return res;
}

status_t EaselManagerClient::suspend() {
    Mutex::Autolock l(mEaselControlLock);
    return suspendLocked();
}

bool EaselManagerClient::isOpenFuturePendingLocked() {
    if (!mOpenFuture.valid()) {
        return false;
    }

    // If open future is valid, check if it is still pending.
    return mOpenFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready;
}

status_t EaselManagerClient::suspendLocked() {
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
    return mEaselControl.suspend();
}

status_t EaselManagerClient::resume() {
    ALOGD("%s: Resuming Easel.", __FUNCTION__);
    Mutex::Autolock l(mEaselControlLock);
    if (!mEaselControlOpened) {
        ALOGE("%s: Easel control is not opened.", __FUNCTION__);
        return NO_INIT;
    }

    SCOPE_PROFILER_TIMER("Resume Easel");
    status_t res = mEaselControl.resume();
    if (res == -EBUSY) {
        return -EUSERS;
    }

    return res;
}

status_t EaselManagerClient::convertCameraId(uint32_t cameraId,
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

status_t EaselManagerClient::startMipi(uint32_t cameraId, uint32_t outputPixelClkHz) {
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

    ALOGD("%s: Start MIPI rate %d for camera %u", __FUNCTION__, rate, cameraId);

    SCOPE_PROFILER_TIMER("Start MIPI");
    res = mEaselControl.startMipi(easelCameraId, rate);
    if (res != OK) {
        ALOGE("%s: Failed to config mipi: %s (%d).", __FUNCTION__, strerror(errno), -errno);
        return NO_INIT;
    }

    return res;
}

status_t EaselManagerClient::stopMipi(uint32_t cameraId) {
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

status_t EaselManagerClient::openHdrPlusClientInternal(HdrPlusClientListener *listener,
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
    auto newClient = std::make_unique<HdrPlusClient>();

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

status_t EaselManagerClient::openHdrPlusClientAsync(HdrPlusClientListener *listener) {
    Mutex::Autolock l(mEaselControlLock);
    if (isOpenFuturePendingLocked()) {
        ALOGE("%s: HDR+ client is already being opened.", __FUNCTION__);
        return ALREADY_EXISTS;
    }

    // Launch a future to open an HDR+ client.
    mOpenFuture = std::async(std::launch::async, &EaselManagerClient::openHdrPlusClientInternal,
            this, listener, /* client */nullptr);

    return OK;
}

status_t EaselManagerClient::openHdrPlusClient(HdrPlusClientListener *listener,
        std::unique_ptr<HdrPlusClient> *client) {
    return openHdrPlusClientInternal(listener, client);
}

void EaselManagerClient::closeHdrPlusClient(std::unique_ptr<HdrPlusClient> client) {
    client = nullptr;

    Mutex::Autolock l(mEaselControlLock);
    status_t res = deactivateLocked();
    if (res != OK) {
        ALOGE("%s: Deactivating Easel failed: %s (%d)", __FUNCTION__, strerror(-res), res);
    }
}

status_t EaselManagerClient::activateLocked() {
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

status_t EaselManagerClient::deactivateLocked() {
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