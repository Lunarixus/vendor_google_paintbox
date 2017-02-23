// #define LOG_NDEBUG 0
#define LOG_TAG "MessengerToPbTiService"
#include <utils/Log.h>

#include "MessengerToPbTiService.h"
#include "PbTiMessageTypes.h"

namespace pbti {

MessengerToPbTiService::MessengerToPbTiService() : mConnected(false) {
}

MessengerToPbTiService::~MessengerToPbTiService() {
    disconnect();
}

status_t MessengerToPbTiService::connect(EaselMessengerListener &listener) {
    std::lock_guard<std::mutex> lock(mApiLock);

    if (mConnected) {
        return -EEXIST;
    }

    status_t res;

#if !USE_LIB_EASEL
    // Only needed for TCP/IP mock
    res = mEaselCommClient.connect(mDefaultServerHost);
    if (res != 0) {
        ALOGE("%s: Connecting to %s failed: %s (%d)", __FUNCTION__,
              mDefaultServerHost, strerror(-errno), errno);
        return -ENODEV;
    }
#endif

    res = mEaselCommClient.open(EaselComm::EASEL_SERVICE_TEST);
    if (res != 0) {
        ALOGE("%s: Opening EaselComm failed: %s (%d)", __FUNCTION__,
              strerror(-res), res);
        return res;
    }

    // Connect to EaselComm
    res = EaselMessenger::connect(listener, kMaxPbTiMessageSize,
                                  &mEaselCommClient);
    if (res != 0) {
        ALOGE("%s: Connecting to EaselComm failed: %s (%d)", __FUNCTION__,
              strerror(-res), res);
        mEaselCommClient.close();
        return res;
    }

    mConnected = true;

    // Connect to paintbox test Service
    res = connectToService();
    if (res != 0) {
        ALOGE("%s: Connecting to paintbox test service failed: %s (%d)",
              __FUNCTION__, strerror(-res), res);
        disconnectWithLockHeld();
        return res;
    }

    return 0;
}

status_t MessengerToPbTiService::connectToService() {
    // Prepare the message.
    Message *message = nullptr;
    status_t res = getEmptyMessage(&message);
    if (res != 0) {
        return res;
    }

    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(MESSAGE_CONNECT));

    // Send to service.
    return sendMessage(message);
}

void MessengerToPbTiService::disconnect() {
    std::lock_guard<std::mutex> lock(mApiLock);
    return disconnectWithLockHeld();
}

status_t MessengerToPbTiService::disconnectFromService() {
    // Prepare the message.
    Message *message = nullptr;
    int res = getEmptyMessage(&message);
    if (res != 0) {
        return res;
    }

    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(MESSAGE_DISCONNECT));

    // Send to service.
    return sendMessage(message);
}

void MessengerToPbTiService::disconnectWithLockHeld() {
    if (!mConnected) return;

    int res = disconnectFromService();
    if (res != 0) {
        ALOGE("%s: Disconnecting from service failed: %s (%d).",
              __FUNCTION__, strerror(-res), res);
    }

    mEaselCommClient.close();
    EaselMessenger::disconnect();
    mConnected = false;
}

status_t MessengerToPbTiService::submitPbTiTestRequest(
  const PbTiTestRequest &request) {
    std::lock_guard<std::mutex> lock(mApiLock);
    if (!mConnected) {
        ALOGE("%s: Not connected to service.", __FUNCTION__);
        return -ENODEV;
    }

    // Prepare the message.
    Message *message = nullptr;
    status_t res = getEmptyMessage(&message);
    if (res != 0) {
        return res;
    }

    RETURN_ERROR_ON_WRITE_ERROR(
      message->writeUint32(MESSAGE_SUBMIT_PBTI_TEST_REQUEST));
    // Serialize test request.
    RETURN_ERROR_ON_WRITE_ERROR(message->writeUint32(request.timeout_seconds));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeString(request.log_path));
    RETURN_ERROR_ON_WRITE_ERROR(message->writeString(request.command));

    // Send to service.
    return sendMessage(message);
}

}  // namespace pbti
