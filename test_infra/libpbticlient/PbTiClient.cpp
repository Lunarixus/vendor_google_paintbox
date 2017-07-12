// #define LOG_NDEBUG 0
#define LOG_TAG "PbTiClient"
#include <log/log.h>

#include <inttypes.h>
#include <stdlib.h>

#include "PbTiClient.h"

namespace android {

PbTiClient::PbTiClient() :
    mEaselControlOpened(false),
    mClientListener(nullptr) {
    mIsEaselPresent = ::isEaselPresent();
    ALOGI("%s: Easel is %s",
          __FUNCTION__, mIsEaselPresent ? "present" : "not present");
}

PbTiClient::~PbTiClient() {
    disconnect();
    mEaselControlOpened = false;
}

bool PbTiClient::isEaselPresentOnDevice() const {
    return mIsEaselPresent;
}

status_t PbTiClient::openEasel() {
    ALOGD("%s: Opening an easelcontrol connection to Easel.", __FUNCTION__);
    Mutex::Autolock l(mEaselControlLock);
    if (mEaselControlOpened) {
        return OK;
    }

    status_t res;

    res = mEaselControl.open();

    if (res != OK) {
        ALOGE("%s: Failed to open Easel control: %s (%d).",
              __FUNCTION__, strerror(errno), -errno);
        return NO_INIT;
    }

    mEaselControlOpened = true;
    return res;
}

void PbTiClient::closeEasel() {
    ALOGD("%s: Closing easelcontrol connection.", __FUNCTION__);
    Mutex::Autolock l(mEaselControlLock);
    if (mEaselControlOpened) {
        mEaselControl.close();
        mEaselControlOpened = false;
    }
}

status_t PbTiClient::activateEasel() {
    ALOGD("%s: Activating Easel.", __FUNCTION__);
    Mutex::Autolock l(mEaselControlLock);
    if (!mEaselControlOpened) {
        ALOGE("%s: Easel control is not opened.", __FUNCTION__);
        return NO_INIT;
    }

    return mEaselControl.activate();
}

status_t PbTiClient::deactivateEasel() {
    ALOGD("%s: Deactivating Easel.", __FUNCTION__);
    Mutex::Autolock l(mEaselControlLock);
    if (!mEaselControlOpened) {
        ALOGE("%s: Easel control is not opened.", __FUNCTION__);
        return NO_INIT;
    }

    return mEaselControl.deactivate();
}

status_t PbTiClient::suspendEasel() {
    ALOGD("%s: Suspending Easel.", __FUNCTION__);
    Mutex::Autolock l(mEaselControlLock);
    if (!mEaselControlOpened) {
        ALOGE("%s: Easel control is not opened.", __FUNCTION__);
        return NO_INIT;
    }

    return mEaselControl.suspend();
}

status_t PbTiClient::resumeEasel() {
    ALOGD("%s: Resuming Easel.", __FUNCTION__);
    Mutex::Autolock l(mEaselControlLock);
    if (!mEaselControlOpened) {
        ALOGE("%s: Easel control is not opened.", __FUNCTION__);
        return NO_INIT;
    }

    return mEaselControl.resume();
}

// TODO: change system call to ioctl function call when it's ready
status_t PbTiClient::freezeEaselState() {
    ALOGD("%s: Freezing Easel state.", __FUNCTION__);
    // TODO(b/62456935): remove "sleep" after the bug is fixed
    sleep(2);
    system("echo 1 > /sys/devices/virtual/misc/mnh_sm/freeze_state");
    return OK;
}

// TODO: change system call to ioctl function call when it's ready
status_t PbTiClient::unfreezeEaselState() {
    ALOGD("%s: Unfreezing Easel state.", __FUNCTION__);
    // TODO(b/62456935): remove "sleep" after the bug is fixed
    sleep(2);
    system("echo 0 > /sys/devices/virtual/misc/mnh_sm/freeze_state");
    return OK;
}

status_t PbTiClient::connect(PbTiClientListener *listener) {
    ALOGV("%s", __FUNCTION__);

    if (listener == nullptr) {
        return BAD_VALUE;
    }

    // Connect to the messenger for sending messages to paintbox test service.
    status_t res = mMessengerToService.connect(*this);
    if (res != OK) {
        ALOGE("%s: Connecting service messenger failed: %s (%d)",
              __FUNCTION__, strerror(-res), res);
        disconnect();
        return res;
    }

    Mutex::Autolock l(mClientListenerLock);
    mClientListener = listener;

    return OK;
}

void PbTiClient::disconnect() {
    ALOGV("%s", __FUNCTION__);

    mMessengerToService.disconnect();

    Mutex::Autolock l(mClientListenerLock);
    mClientListener = nullptr;
}

status_t PbTiClient::submitPbTiTestRequest(
  const pbti::PbTiTestRequest &request) {
    ALOGV("%s: submit a test request.", __FUNCTION__);

    // Send the request to paintbox test service.
    status_t res = mMessengerToService.submitPbTiTestRequest(request);
    if (res != 0) {
        ALOGE("%s: Sending test request to test service failed: %s (%d).",
              __FUNCTION__, strerror(-res), res);
        return res;
    }

    return 0;
}

// Callbacks from paintbox test service.
void PbTiClient::notifyPbTiTestResult(const std::string &result) {
    ALOGV("%s: Got an easel test result.", __FUNCTION__);

    // Invoke client listener callback for the test result.
    mClientListener->onPbTiTestResult(result);
}

void PbTiClient::notifyPbTiTestResultFailed() {
    ALOGE("%s: Failed to get easel test result.", __FUNCTION__);

    // Invoke client listener callback for the test result.
    mClientListener->onPbTiTestResultFailed();
}

}  // namespace android
