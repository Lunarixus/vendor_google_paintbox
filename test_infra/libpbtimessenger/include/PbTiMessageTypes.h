#ifndef PAINTBOX_PB_TI_MESSAGE_TYPES_H
#define PAINTBOX_PB_TI_MESSAGE_TYPES_H

#include <stdint.h>

namespace pbti {

// This file defines the types used in messages passed by EaselMessenger.

// Maximum message size passed between paintbox test client and service.
const int kMaxPbTiMessageSize = 4096;

/*
 * PbTiMessageType defines the message types that can be passed between
 * paintbox test service and client.
 */
enum PbTiMessageType {
    // Messages from paintbox test client to paintbox test service
    MESSAGE_CONNECT = 0,
    MESSAGE_DISCONNECT,
    MESSAGE_SUBMIT_PBTI_TEST_REQUEST,

    // Messages from paintbox test service to paintbox test client
    MESSAGE_NOTIFY_TEST_RESULT = 0x10000,
};

}  // namespace pbti

#endif  // PAINTBOX_PB_TI_MESSAGE_TYPES_H
