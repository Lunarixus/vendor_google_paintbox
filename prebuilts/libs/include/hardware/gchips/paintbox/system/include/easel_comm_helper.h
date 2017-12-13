#ifndef HARDWARE_GCHIPS_PAINTBOX_SYSTEM_INCLUDE_EASEL_COMM_HELPER_H_
#define HARDWARE_GCHIPS_PAINTBOX_SYSTEM_INCLUDE_EASEL_COMM_HELPER_H_

#include <functional>
#include <memory>

// When protobuf is not supported in easelcomm,
// Uses proto header from Android platform instead.
#ifndef EASEL_PROTO_SUPPORT
#include <google/protobuf/message_lite.h>
#endif  // EASEL_PROTO_SUPPORT

#include "hardware/gchips/paintbox/system/include/easel_comm.h"

namespace easel {

using MessageHandlerFunction = std::function<void(const Message&)>;

// A helper to generate a handler that runs a std::function to handle receive
// message.
// Comm* is ignored in MessageHandlerFunction since user could use the Comm
// object from the context.
// This helper is available in Android.
class FunctionHandler : public MessageHandler {
 public:
  explicit FunctionHandler(const MessageHandlerFunction& function)
      : function_(function) {}
  void OnReceive(const Message& message, Comm*) override { function_(message); }

 private:
  MessageHandlerFunction function_;
};

// Helper function to create HardwareBuffer unique_ptr with vaddr.
inline std::unique_ptr<HardwareBuffer> CreateHardwareBuffer(void* vaddr,
                                                            size_t size,
                                                            int id = 0) {
  return std::unique_ptr<HardwareBuffer>(
      HardwareBuffer::Create(vaddr, size, id));
}

// Helper function to create HardwareBuffer unique_ptr with ion_fd.
inline std::unique_ptr<HardwareBuffer> CreateHardwareBuffer(int ion_fd,
                                                            size_t size,
                                                            int id = 0) {
  return std::unique_ptr<HardwareBuffer>(
      HardwareBuffer::Create(ion_fd, size, id));
}

// Helper function to allocate HardwareBuffer unique_ptr with size.
inline std::unique_ptr<HardwareBuffer> AllocateHardwareBuffer(size_t size,
                                                              int id = 0) {
  return std::unique_ptr<HardwareBuffer>(HardwareBuffer::Allocate(size, id));
}

// Helper function to create ping Message unique_ptr.
inline std::unique_ptr<Message> CreateMessage(
    int channel_id, const HardwareBuffer* payload = nullptr) {
  return std::unique_ptr<Message>(Message::Create(channel_id, payload));
}

// Helper function to create raw buffer Message unique_ptr.
inline std::unique_ptr<Message> CreateMessage(
    int channel_id, const void* body, size_t size,
    const HardwareBuffer* payload = nullptr) {
  return std::unique_ptr<Message>(
      Message::Create(channel_id, body, size, payload));
}

// Helper function to create struct Message unique_ptr.
template <typename T>
inline std::unique_ptr<Message> CreateMessage(
    int channel_id, const T* body, const HardwareBuffer* payload = nullptr) {
  return CreateMessage(channel_id, body, sizeof(T), payload);
}

// Helper function to create Comm Message unique_ptr.
inline std::unique_ptr<Comm> CreateComm(Comm::Type type) {
  return std::unique_ptr<Comm>(Comm::Create(type));
}

#ifdef EASEL_PROTO_SUPPORT
// Helper function to create proto Message unique_ptr.
inline std::unique_ptr<Message> CreateMessage(
    int channel_id, const MessageLite& proto,
    const HardwareBuffer* payload = nullptr) {
  return std::unique_ptr<Message>(Message::Create(channel_id, proto, payload));
}
#else
// Provides protobuf support as inline function in Android.
// This is to walk around the protobuf version mismatch between Android and
// google3.
using ::google::protobuf::MessageLite;

// Helper function to send protobuf with libeaselsystem.so
// Returns 0 if successful, otherwise error code.
inline int SendProto(Comm* comm, int channel_id, const MessageLite& proto,
                     const HardwareBuffer* payload = nullptr) {
  size_t size = proto.ByteSize();
  void* proto_buffer = malloc(size);
  if (proto_buffer == nullptr) return -ENOMEM;
  if (!proto.SerializeToArray(proto_buffer, size)) return -EINVAL;
  return comm->Send(channel_id, proto_buffer, size, payload);
}

// Helper function to convert Message to protobuf in libeaselsystem.so
// Returns true if successful, otherwise false.
inline bool MessageToProto(const Message& message, MessageLite* proto) {
  return proto->ParseFromArray(message.GetBody(), message.GetBodySize());
}
#endif  // EASEL_PROTO_SUPPORT

}  // namespace easel

#endif  // HARDWARE_GCHIPS_PAINTBOX_SYSTEM_INCLUDE_EASEL_COMM_HELPER_H_
