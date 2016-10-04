#ifndef ANDROID_EASELCONTROL_IMPL_H
#define ANDROID_EASELCONTROL_IMPL_H

/*
 * Implements the public EaselControlClient/Server interfaces.
 */

#include <stdint.h>
#include <sys/time.h>

class EaselControlImpl {
public:
#ifdef MOCKEASEL
  // TCP/IP mock version uses this default TCP port
  static const int kDefaultMockSysctrlPort = 4243;
#endif

  // Control commands for the EaseControlImpl layer
  enum Command {
      CMD_DEACTIVATE,      // Deactivate Easel
      CMD_SET_TIME,        // Sync AP boottime and time of day clocks
      CMD_LOG,             // Android logging string
  };

  // All control messages start with this header
  struct MsgHeader {
      uint32_t command;    // an enum Command code
  };

  // CMD_SET_TIME message, from client to server
  struct SetTimeMsg {
      struct MsgHeader h;   // common header
      uint64_t boottime;    // AP boottime clock
      uint64_t realtime;    // AP realtime time of day clock
  };

  // CMD_LOG message, from server to client
  struct LogMsg {
      struct MsgHeader h;   // common header
      uint32_t prio;        // __android_log_write priority
      uint32_t tag_len;     // length of tag including terminator (bytes)
      // followed by null-terminated tag string
      // followed by null-terminated text string
  };

  // CMD_DEACTIVATE message, no further info beyond command
  struct DeactivateMsg {
      struct MsgHeader h;   // common header
  };

};
#endif // ANDROID_EASELCONTROL_IMPL_H