// #define LOG_NDEBUG 0
#define LOG_TAG "PbTiClient"
#include <utils/Log.h>

#include <inttypes.h>
#include <stdlib.h>

#include "PbTiClient.h"

namespace android {

PbTiClient::PbTiClient() : mEaselActivated(false), mClientListener(nullptr) {
}

PbTiClient::~PbTiClient() {
    disconnect();
}

status_t PbTiClient::connect(PbTiClientListener *listener) {
    ALOGV("%s", __FUNCTION__);

    if (listener == nullptr) {
        return BAD_VALUE;
    }

    status_t res = OK;

    res = activateEasel();
    if (res != OK) {
        ALOGE("%s: Activating Easel failed: %s (%d).",
              __FUNCTION__, strerror(-res), res);
        return res;
    }

    // Connect to the messenger for sending messages to paintbox test service.
    res = mMessengerToService.connect(*this);
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
    mClientListener = nullptr;
    deactivateEasel();
}

status_t PbTiClient::activateEasel() {
    Mutex::Autolock l(mEaselControlLock);
    if (mEaselActivated) {
        ALOGE("%s: Easel is already activated.", __FUNCTION__);
        return ALREADY_EXISTS;
    }

    status_t res;
    // Open Easel control.
#if !USE_LIB_EASEL
    res = mEaselControl.open(mDefaultServerHost);
#else
    res = mEaselControl.open();
#endif
    if (res != OK) {
        ALOGE("%s: Failed to open Easel control: %s (%d).",
              __FUNCTION__, strerror(errno), -errno);
        return NO_INIT;
    }

    // Activate Easel.
    res = mEaselControl.activateEasel();
    if (res != OK) {
        ALOGE("%s: Failed to activate Easel: %s (%d).",
              __FUNCTION__, strerror(errno), -errno);
        mEaselControl.close();
        return NO_INIT;
    }
    mEaselActivated = true;

    return OK;
}

void PbTiClient::deactivateEasel() {
    ALOGV("%s: deactivate Easel.", __FUNCTION__);

    Mutex::Autolock l(mEaselControlLock);
    if (!mEaselActivated) {
        return;
    }

    mEaselControl.deactivateEasel();
    mEaselControl.close();
    mEaselActivated = false;
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

}  // namespace android