#ifndef PAINTBOX_EASEL_COMM_UTIL_H
#define PAINTBOX_EASEL_COMM_UTIL_H

#include <google/protobuf/message_lite.h>

#include "hardware/gchips/paintbox/system/include/easel_comm.h"

namespace easel {

using ::google::protobuf::MessageLite;

// Util function to send protobuf with libeaselsystem.so
// Returns 0 if successful, otherwise error code.
inline int SendProto(Comm* comm, int channel_id, const MessageLite& proto,
                     const HardwareBuffer* payload = nullptr) {
  size_t size = proto.ByteSize();
  void* proto_buffer = malloc(size);
  if (proto_buffer == nullptr) return -ENOMEM;
  if (!proto.SerializeToArray(proto_buffer, size)) return -EINVAL;
  return comm->Send(channel_id, proto_buffer, size, payload);
}

// Util function to convert Message to protobuf in libeaselsystem.so
// Returns true if successful, otherwise false.
inline bool MessageToProto(const Message& message, MessageLite* proto) {
  return proto->ParseFromArray(message.GetBody(), message.GetBodySize());
}

}  // namespace easel

#endif  // PAINTBOX_EASEL_COMM_UTIL_H
