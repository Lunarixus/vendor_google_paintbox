// #define LOG_NDEBUG 0
#define LOG_TAG "MessengerListenerFromPbTiClient"
#include <log/log.h>

#include "PbTiMessageTypes.h"
#include "MessengerListenerFromPbTiClient.h"

namespace pbti {

status_t MessengerListenerFromPbTiClient::onMessage(Message *message) {
    if (message == nullptr) {
        return -EINVAL;
    }

    uint32_t type = 0;

    RETURN_ERROR_ON_READ_ERROR(message->readUint32(&type));
    ALOGV("%s: Got message %u", __FUNCTION__, type);
    switch (type) {
        case MESSAGE_CONNECT:
            return connect();
        case MESSAGE_DISCONNECT:
            disconnect();
            return 0;
        case MESSAGE_SUBMIT_PBTI_TEST_REQUEST:
            return deserializeSubmitPbTiTestRequest(message);
        default:
            ALOGE("%s: Received invalid message type %d.", __FUNCTION__, type);
            return -EINVAL;
    }

    return 0;
}

status_t MessengerListenerFromPbTiClient::deserializeSubmitPbTiTestRequest(
  Message *message) {
    PbTiTestRequest request;

    // Deserialize PbTi test request
    RETURN_ERROR_ON_READ_ERROR(message->readUint32(&request.timeout_seconds));
    RETURN_ERROR_ON_READ_ERROR(message->readString(&request.log_path));
    RETURN_ERROR_ON_READ_ERROR(message->readString(&request.command));

    return submitPbTiTestRequest(request);
}

}  // namespace pbti
