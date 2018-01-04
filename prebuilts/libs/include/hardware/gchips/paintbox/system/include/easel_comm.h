#ifndef HARDWARE_GCHIPS_PAINTBOX_SYSTEM_INCLUDE_EASEL_COMM_H_
#define HARDWARE_GCHIPS_PAINTBOX_SYSTEM_INCLUDE_EASEL_COMM_H_

#include <stddef.h>

// There is a mismatch between google3(3.4) and Android platform(3.0) protobuf
// version. So protobuf support when linked in Android source is achieved
// through an inline helper defined in easel_comm_helper.h.
// If you build your executable in google3, EASEL_PROTO_SUPPORT is defined by
// default. You could always use protobuf API in easelcomm.
#ifdef EASEL_PROTO_SUPPORT
#include "net/proto2/public/message_lite.h"
#endif  // EASEL_PROTO_SUPPORT

namespace easel {

#ifdef EASEL_PROTO_SUPPORT
using proto2::MessageLite;
#endif  // EASEL_PROTO_SUPPORT

// Easel service identifiers registered by clients and servers to
// route messages to each other.
enum EaselService {
  // Easel system control
  EASEL_SERVICE_SYSCTRL = 0,
  // Easel shell
  EASEL_SERVICE_SHELL,
  // Used by unit tests
  EASEL_SERVICE_TEST,
  // HDR+ via Paintbox camera framework service
  EASEL_SERVICE_HDRPLUS,
  // Logging service.
  EASEL_SERVICE_LOG,
  // NN service
  EASEL_SERVICE_NN,
  // EaselManager service
  EASEL_SERVICE_MANAGER,
  // EaselManager system control
  EASEL_SERVICE_MANAGER_SYSCTRL,
  // Invalid service
  EASEL_SERVICE_UNKNOWN,
};

// Abstraction of device buffers supported in EaselComm2
// for buffer transferring on PCIe.
// Buffer could be specified either by vaddr or ion fd.
// TODO(b/69793498): Add buffer lock and unlock methods.
class HardwareBuffer {
 public:
  HardwareBuffer(const HardwareBuffer&) = delete;
  HardwareBuffer& operator=(const HardwareBuffer&) = delete;

  // Wraps a malloc hardware buffer.
  // Returns the pointer to HardwareBuffer if successful and nullptr if
  // failed. Caller is expected to take ownership of the allocated
  // HardwareBuffer object, for example, converting it with a unique_ptr.
  // The created HardwareBuffer is returned as raw pointer instead of unique_ptr
  // for ABI compatibility concern.
  // vaddr is still owned by caller.
  static HardwareBuffer* Create(void* vaddr, size_t size, int id = 0);

  // Wraps an ion hardware buffer.
  // Returns the pointer to HardwareBuffer if successful and nullptr if
  // failed. Caller is expected to take ownership of the allocated
  // HardwareBuffer object, for example, converting it with a unique_ptr.
  static HardwareBuffer* Create(int ion_fd, size_t size, int id = 0);

  // Allocates a malloc hardware buffer with specified buffer size.
  // Returns the pointer to HardwareBuffer if successful and nullptr if
  // failed. Caller is expected to take ownership of the allocated
  // HardwareBuffer object, for example, converting it with a unique_ptr.
  // The internal malloc buffer is owned by the HardwareBuffer object and will
  // be freed in HardwareBuffer destructor.
  static HardwareBuffer* Allocate(size_t size, int id = 0);

  virtual ~HardwareBuffer();

  // Returns true if buffer is valid and not empty, otherwise false.
  virtual bool Valid() const = 0;

  // Returns true if HardwareBuffer is ion buffer based, otherwise false.
  virtual bool IsIonBuffer() const = 0;

  // Returns the mutable vaddr of the buffer, nullptr if it is not malloc
  // buffer.
  virtual void* GetVaddrMutable() = 0;

  // Returns the immutable vaddr of the buffer, nullptr if it is not malloc
  // buffer.
  virtual const void* GetVaddr() const = 0;

  // Returns the ion fd of the buffer or -1 if it is not ion buffer.
  virtual int GetIonFd() const = 0;

  // Returns the size of the buffer.
  virtual size_t GetSize() const = 0;

  // Returns the id of the buffer.
  virtual int GetId() const = 0;

  // Sets the id of the buffer.
  virtual void SetId(int id) = 0;

 protected:
  HardwareBuffer();
};

// EaselComm2::Message that supports conversion from the following types:
// 1) raw pointer
// 2) proto buffer
// 3) empty message as a ping
// EaselComm2::Message also supports appending an optional image buffer payload.
class Message {
 public:
  // Type of the message.
  enum Type {
    RAW = 0,
    PROTO = 1,
    PING = 2,
  };

  Message(const Message&) = delete;
  Message& operator=(const Message&) = delete;

  // Creates an Message with empty body and an optional payload.
  // Caller is expected to take ownership of the allocated
  // Message object.
  static Message* Create(int channel_id,
                         const HardwareBuffer* payload = nullptr);

#ifdef EASEL_PROTO_SUPPORT
  // Creates an Message with protobuf and an optional payload.
  // Caller is expected to take ownership of the allocated
  // Message object.
  static Message* Create(int channel_id, const MessageLite& proto,
                         const HardwareBuffer* payload = nullptr);
#endif  // EASEL_PROTO_SUPPORT

  // Creates an Message with raw buffer and an optional payload.
  // Caller is expected to take ownership of the allocated
  // Message object.
  static Message* Create(int channel_id, const void* body, size_t size,
                         const HardwareBuffer* payload = nullptr);

  // Creates an Message with struct and an optional payload.
  // Caller is expected to take ownership of the allocated
  // Message object.
  template <typename T>
  static Message* Create(int channel_id, const T* body,
                         const HardwareBuffer* payload = nullptr) {
    return Create(channel_id, body, sizeof(T), payload);
  }

  virtual ~Message();

#ifdef EASEL_PROTO_SUPPORT
  // Converts the message to proto buffer.
  // Returns true if successful, otherwise empty string.
  virtual bool ToProto(MessageLite* proto) const = 0;
#endif  // EASEL_PROTO_SUPPORT

  // Converts the message to a struct T
  // Returns pointer to T if successful otherwise nullptr.
  // This conversion is zero-copy.
  template <typename T>
  const T* ToStruct() const {
    if (GetType() != RAW || sizeof(T) != GetBodySize()) {
      return nullptr;
    }
    return reinterpret_cast<const T*>(GetBody());
  }

  // Returns the channel id of the message.
  virtual int GetChannelId() const = 0;

  // Returns the type of the message.
  virtual Type GetType() const = 0;

  // Returns the id of the payload carried by the message.
  // Default is 0.
  virtual int GetPayloadId() const = 0;

  // Returns the body address of this message.
  virtual const void* GetBody() const = 0;

  // Returns the size of the message body in bytes.
  virtual size_t GetBodySize() const = 0;

  // Returns the size of the DMA payload in bytes.
  virtual size_t GetPayloadSize() const = 0;

 protected:
  Message();
};

class Comm;

// A message handler class to handle incoming messages.
// Ideally, we could simply just use std::function, but since google3 / Android
// does not share the same c++ stl library, so we have to use an interface.
class MessageHandler {
 public:
  MessageHandler(const MessageHandler&) = delete;
  MessageHandler& operator=(const MessageHandler&) = delete;

  virtual ~MessageHandler() {}

  // Handles a received message, called when message is received.
  // message will be destroyed after this function is returned.
  // comm is also provided for convenience of sending reply back or receive
  // payload.
  virtual void OnReceive(const Message& message, Comm* comm) = 0;

 protected:
  MessageHandler() {}
};

// Communication instance for sending and receiving messages between Android AP
// and Easel co-processor.
// TODO(cjluo): Finish the class description once we ported all the
// functionalities.
class Comm {
 public:
  // Type indicate whether the communication instance is server or client.
  enum class Type {
    CLIENT,
    SERVER,
  };

  Comm(const Comm&) = delete;
  Comm& operator=(const Comm&) = delete;

  virtual ~Comm();

  // Opens communications for the specified service.
  // service_id the id of the service channel. Must match on server and client.
  // timeout_ms timeout value to wait for connection.
  virtual int Open(EaselService service_id, long timeout_ms) = 0;  // NOLINT

  // Opens communications for the specified service with default timeout.
  // service_id the id of the service channel. Must match on server and client.
  virtual int Open(EaselService service_id) = 0;

  // Opens communications for the specified service.
  // When the link is down (the other side close the communication or powers
  // off), close the link and reopen again. If reopen fails, the function will
  // return the error code. This function will also start and join handler
  // thread. This function will block forever and never return unless open
  // fails. logging specifies if the open / close logging is turned on.
  virtual int OpenPersistent(EaselService service_id, bool logging) = 0;

  // Closes down communication via this object.
  virtual void Close() = 0;

  // Returns true when connection is established, otherwise false.
  virtual bool IsUp() = 0;

  // Creates a Comm object. Returns the pointer if successful and nullptr if
  // failed. Caller is expected to take ownership of the allocated Comm object,
  // for example, converting it with a unique_ptr.
  static Comm* Create(Type type);

  // Starts the receiving thread.
  // Handler thread will call registered handler OnReceive to handle messages.
  // Returns 0 if successful, else the error code.
  virtual int StartReceiving() = 0;

  // Joins the receiving thread.
  virtual void JoinReceiving() = 0;

  // Sends an empty message and an optional payload to the other side.
  // payload could be nullable.
  // Returns 0 if successful, otherwise the error code.
  virtual int Send(int channel_id, const HardwareBuffer* payload) = 0;

  // Sends a struct and an optional payload to the other side.
  // payload could be nullable.
  // Returns 0 if successful, otherwise the error code.
  virtual int Send(int channel_id, const void* body, size_t body_size,
                   const HardwareBuffer* payload) = 0;

#ifdef EASEL_PROTO_SUPPORT
  // Sends a protobuf and an optional payload to the other side.
  // payload could be nullable.
  // Returns 0 if successful, otherwise the error code.
  virtual int Send(int channel_id, const MessageLite& proto,
                   const HardwareBuffer* payload) = 0;
#endif  // EASEL_PROTO_SUPPORT

  // Sends a struct and an optional payload to the other side.
  // Returns 0 if successful, otherwise the error code.
  // payload could be nullable.
  template <typename T>
  int SendStruct(int channel_id, const T& body, const HardwareBuffer* payload) {
    return Send(channel_id, &body, sizeof(T), payload);
  }

  // Receives the HardwareBuffer payload in DMA to buffer.
  // Returns the error code.
  // Could be called inside handler function.
  // If buffer is nullptr, it will cancel the current DMA buffer
  // It will also override the buffer id to match the source buffer id.
  virtual int ReceivePayload(const Message& message,
                             HardwareBuffer* buffer) = 0;

  // Cancels receiving the DMA payload.
  // Returns the error code.
  virtual int CancelPayload(const Message& message) = 0;

  // Registers a message handler to channel_id.
  virtual void RegisterHandler(int channel_id, MessageHandler* handler) = 0;

 protected:
  Comm();
};

}  // namespace easel
#endif  // HARDWARE_GCHIPS_PAINTBOX_SYSTEM_INCLUDE_EASEL_COMM_H_
