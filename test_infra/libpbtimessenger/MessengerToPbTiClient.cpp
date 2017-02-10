// #define LOG_NDEBUG 0
#define LOG_TAG "MessengerToPbTiClient"
#include "Log.h"

#include "MessengerToPbTiClient.h"

namespace pbti {

MessengerToPbTiClient::MessengerToPbTiClient() : mConnected(false) {
}

MessengerToPbTiClient::~MessengerToPbTiClient() {
    disconnect();
}

status_t MessengerToPbTiClient::connect(EaselMessengerListener &listener) {
    std::lock_guard<std::mutex> lock(mApiLock);

    if (mConnected) {
        return -EEXIST;
    }

    // Connect to paintbox test service.
    status_t res = mEaselCommServer.open(EaselComm::EASEL_SERVICE_TEST);
    if (res != 0) {
        ALOGE("%s: Opening EaselCommServer failed: %s (%d).",
              __FUNCTION__, strerror(-res), res);
        return -ENODEV;
    }

    res = EaselMessenger::connect(listener,
                                  kMaxPbTiMessageSize, &mEaselCommServer);
    if (res != 0) {
        ALOGE("%s: Connecting EaselMessenger failed: %s (%d).", __FUNCTION__,
              strerror(-res), res);
        mEaselCommServer.close();
        return res;
    }

    mConnected = true;
    return 0;
}

void MessengerToPbTiClient::disconnect() {
    std::lock_guard<std::mutex> lock(mApiLock);

    if (!mConnected) {
        return;
    }

    mEaselCommServer.close();
    EaselMessenger::disconnect();
}

void MessengerToPbTiClient::notifyPbTiTestResult(const std::string &result) {
    std::lock_guard<std::mutex> lock(mApiLock);

    if (!mConnected) {
        ALOGE("%s: Messenger not connected.", __FUNCTION__);
        return;
    }

    // Prepare the message.
    Message *message = nullptr;
    status_t res = getEmptyMessage(&message);
    if (res != 0) {
        ALOGE("%s: Getting empty message failed: %s (%d).", __FUNCTION__,
              strerror(-res), res);
    }

    RETURN_ON_WRITE_ERROR(message->writeUint32(MESSAGE_NOTIFY_TEST_RESULT));
    RETURN_ON_WRITE_ERROR(message->writeString(result));

    // Send to client.
    res = sendMessage(message, /*async*/true);
    if (res != 0) {
        ALOGE("%s: Sending message failed: %s (%d).", __FUNCTION__,
              strerror(-res), res);;
    }
}

}  // namespace pbti
