#ifndef ANDROID_EASELCONTROL_IMPL_H
#define ANDROID_EASELCONTROL_IMPL_H

/*
 * Implements the public EaselControlClient/Server interfaces.
 */

#include <stdint.h>
#include <string>
#include <sys/time.h>

#include "easelcomm.h"

class EaselControlImpl {
public:
  // Control commands for the EaseControlImpl layer
  enum Command {
      CMD_ACTIVATE,        // Activate Easel
      CMD_DEACTIVATE,      // Deactivate Easel
      CMD_SUSPEND,         // Suspend Easel
      CMD_SET_TIME,        // Sync AP boottime and time of day clocks
      CMD_RESET_REQ,       // Reset request sent from server to client
      CMD_HEARTBEAT,       // Periodic heartbeat sent from server to client
  };

  enum ReplyCode {
      REPLY_ACTIVATE_OK = 200,
      REPLY_DEACTIVATE_OK = 201,
      REPLY_SUSPEND_OK,
      REPLY_SET_TIME_OK,
  };

  // All control messages start with this header
  struct MsgHeader {
      uint32_t command;    // an enum Command code
  };

  // CMD_ACTIVATE message, includes timestamp info from SetTimeMsg
  struct ActivateMsg {
      struct MsgHeader h;   // common header
      uint64_t boottime;    // AP boottime clock
      uint64_t realtime;    // AP realtime time of day clock
  };

  // CMD_DEACTIVATE message, no further info beyond command
  struct DeactivateMsg {
      struct MsgHeader h;   // common header
  };

  // CMD_SUSPEND message, no further info beyond command
  struct SuspendMsg {
      struct MsgHeader h;   // common header
  };

  // CMD_SET_TIME message, from client to server
  struct SetTimeMsg {
      struct MsgHeader h;   // common header
      uint64_t boottime;    // AP boottime clock
      uint64_t realtime;    // AP realtime time of day clock
  };

  // CMD_HEARTBEAT message, from client to server
  struct HeartbeatMsg {
      struct MsgHeader h;   // common header
      uint32_t seqNumber;   // sequence number
  };

};
#endif // ANDROID_EASELCONTROL_IMPL_H
