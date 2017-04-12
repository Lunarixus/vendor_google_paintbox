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
      CMD_DEACTIVATE,      // Deactivate Easel
      CMD_SET_TIME,        // Sync AP boottime and time of day clocks
      CMD_LOG,             // Android logging string
      CMD_SUSPEND,         // Suspend Easel
      CMD_RPC,             // RPC message, wrapping request and response
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

  static const int kMaxPayloadSize = 4096;

  // CMD_RPC message, struct RpcMsg only specifies msg head.
  // Body will be sent through DMA.
  struct RpcMsg {
      struct MsgHeader h;
      int handlerId;       // Identifies the right handler,
                            // suggest to use CRC32 of the handler name.
      int rpcId;           // RPC Id used for handler to distinguish difference
                            // services within this handler.
      uint64_t callbackId; // Unique id of the callback registered,
                            // 0 if callback not registered.

      uint64_t payloadSize;
      char payloadBody[kMaxPayloadSize];
      RpcMsg() : h({CMD_RPC}) {};

      RpcMsg(const RpcMsg &msg) {
          this->h = msg.h;
          this->handlerId = msg.handlerId;
          this->rpcId = msg.rpcId;
          this->callbackId = msg.callbackId;
          this->payloadSize = 0;
      }

      void getEaselMessage(EaselComm::EaselMessage *msg) {
          msg->message_buf = this;
          msg->message_buf_size = sizeof(RpcMsg) - kMaxPayloadSize + payloadSize;
          msg->dma_buf = nullptr;
          msg->dma_buf_size = 0;
      }
  };
};
#endif // ANDROID_EASELCONTROL_IMPL_H
