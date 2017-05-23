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
#ifdef MOCKEASEL
  // TCP/IP mock version uses this default TCP port
  static const int kDefaultMockSysctrlPort = 4243;
#endif

  // Control commands for the EaseControlImpl layer
  enum Command {
      CMD_ACTIVATE,        // Activate Easel
      CMD_DEACTIVATE,      // Deactivate Easel
      CMD_SUSPEND,         // Suspend Easel
      CMD_SET_TIME,        // Sync AP boottime and time of day clocks
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
};
#endif // ANDROID_EASELCONTROL_IMPL_H
