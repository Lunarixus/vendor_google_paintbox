// #define LOG_NDEBUG 0
#define LOG_TAG "MessengerListenerFromPbTiService"
#include <log/log.h>

#include "PbTiMessageTypes.h"
#include "MessengerListenerFromPbTiService.h"

namespace pbti {

status_t MessengerListenerFromPbTiService::onMessage(Message *message) {
    if (message == nullptr) {
        return -EINVAL;
    }

    uint32_t type = 0;
    RETURN_ERROR_ON_READ_ERROR(message->readUint32(&type));

    switch (type) {
        case MESSAGE_NOTIFY_TEST_RESULT:
            deserializeNotifyTestResult(message);
            return 0;
        default:
            ALOGE("%s: Receive invalid message type %d.", __FUNCTION__, type);
            return -EINVAL;
    }

    return 0;
}

void MessengerListenerFromPbTiService::deserializeNotifyTestResult(
  Message *message) {
    std::string pbtiTestResult;
    RETURN_ON_READ_ERROR(message->readString(&pbtiTestResult));

    notifyPbTiTestResult(pbtiTestResult);
}

}  // namespace pbti
