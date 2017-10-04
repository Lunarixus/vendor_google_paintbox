#ifndef PAINTBOX_EASELCOMM2_H
#define PAINTBOX_EASELCOMM2_H

#include <functional>
#include <vector>

#include "EaselComm2Message.h"
#include "EaselService.h"

namespace EaselComm2 {

// Communication instance for sending messages to the other side.
// Messages support string, struct, and protobuffer
// with an optional HardwareBuffer payload.
class Comm {
 public:
  enum class Mode {
    CLIENT,
    SERVER,
  };

  using Handler = std::function<void(const Message& message)>;

  Comm(const Comm&) = delete;
  Comm& operator=(const Comm&) = delete;

  virtual ~Comm();

  // Opens communications for the specified service.
  // service_id the id of the service channel. Must match on server and client.
  // timeout_ms timeout value to wait for connection.
  virtual int open(EaselService service_id, long timeout_ms) = 0;

  // Opens communications for the specified service with default timeout.
  // service_id the id of the service channel. Must match on server and client.
  virtual int open(EaselService service_id) = 0;

  // Opens communications for the specified service.
  // When the link is down, close the link and reopen again.
  // This function will also start and join handler thread.
  // This function will block forever and never return.
  // retryMS specifies the waittime before every open retry after failure.
  // logging specifies if the open / close logging is turned on.
  virtual void openPersistent(EaselService service_id,
                              int retryMs = 1000,
                              bool logging = true) = 0;

  // Closes down communication via this object.
  virtual void close() = 0;

  // Starts the receiving thread.
  // Handler thread will call overridden onReceive to handle messages
  // Returns the error code.
  virtual int startReceiving() = 0;

  // Joins the receiving thread.
  virtual void joinReceiving() = 0;

  // -------------------------------------------------------
  // Sends a struct and an optional payload to the other side.
  // Returns the error code.
  virtual int send(int channelId, const void* body, size_t body_size,
                   const HardwareBuffer* payload = nullptr) = 0;

  // Sends a string and an optional payload to the other side.
  // Returns the error code.
  virtual int send(int channelId, const std::string& s,
                   const HardwareBuffer* payload = nullptr) = 0;

  // Sends a protobuf and an optional payload to the other side.
  // Returns the error code.
  virtual int send(int channelId, const ::google::protobuf::MessageLite& proto,
                   const HardwareBuffer* payload = nullptr) = 0;

  // Sends a group of buffers as payloads to the other side.
  // Returns the error code and remaining buffers will not be sent if previous
  // buffer fails in sending. lastId will be set to the latest successful
  // buffer id if not null.
  virtual int send(int channelId, const std::vector<HardwareBuffer>& buffers,
                   int* lastId = nullptr) = 0;

  // -------------------------------------------------------
  // Registers a message handler to channelId.
  virtual void registerHandler(int channelId, Handler handler) = 0;

  // Receives the HardwareBuffer payload in DMA to buffer.
  // Returns the error code.
  // Could be called inside handler function.
  // If buffer is nullptr, it will flush the current DMA buffer
  // It will also override the buffer id to match the source buffer id.
  virtual int receivePayload(const Message& message,
                             HardwareBuffer* buffer) = 0;

  // Returns an Comm instance.
  // instance could be either client or server based on mode.
  static std::unique_ptr<Comm> create(Mode mode);

 protected:
  Comm();
};

}  // namespace EaselComm2
#endif  // PAINTBOX_EASELCOMM2_H
