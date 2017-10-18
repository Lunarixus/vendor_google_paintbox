#ifndef PAINTBOX_EASELCOMM2_MESSAGE
#define PAINTBOX_EASELCOMM2_MESSAGE

#include <string>

#include <google/protobuf/message_lite.h>

namespace EaselComm2 {

// Abstraction of device buffers supported in EaselComm2
// for buffer transfering on PCIe.
// Data structure is similar to hidl_memory.
// Buffer could be specified either by vaddr or ionFd.
// If both are valid, vaddr will override ionFd.
struct HardwareBuffer {
  void* vaddr;
  int ionFd;
  size_t size;
  int id;  // optional buffer id to note transferring sequence.

  HardwareBuffer();
  HardwareBuffer(void* vaddr, size_t size, int id = 0);
  HardwareBuffer(int ionFd, size_t size, int id = 0);

  // Returns true if HardwareBuffer is ion based, otherwise false.
  bool isIonBuffer();

  // Loads file into HardwareBuffer
  // Returns buffer vaddr if successful, otherwise nullptr.
  // User needs to free the returned buffer after use.
  void* loadFile(const std::string& filePath);

  // Saves buffer into a file.
  // Returns 0 if successful, otherwise error code.
  int saveFile(const std::string& filePath);
};

// EaselComm2::Message that supports conversion from the following types:
// 1) raw pointer
// 2) string
// 3) proto buffer
// EaselComm2::Message also supports appending an optional image buffer payload.
class Message {
 public:
  // Type of the message.
  enum Type {
    RAW = 0,
    STRING = 1,
    PROTO = 2,
    PING = 3,
  };

  // Message header.
  struct Header {
    int channelId;  // Message channel ID.
    Type type;      // Message type.
    int payloadId;  // Payload ID to note buffer sequence.
  };

  Message(int channelId, const std::string& s,
          const HardwareBuffer* payload = nullptr);

  Message(int channelId, const ::google::protobuf::MessageLite& proto,
          const HardwareBuffer* payload = nullptr);

  Message(int channelId, const void* body, size_t size,
          const HardwareBuffer* payload = nullptr);

  Message(int channelId, const HardwareBuffer* payload = nullptr);

  Message(void* messageBuf, size_t messageBufSize, size_t dmaBufSize,
          uint64_t messageId);

  ~Message();

  // Converts message to string.
  // Returns the converted string if successful, otherwise empty string.
  std::string toString() const;

  // Converts the message to proto buffer.
  // Returns true if successful, otherwise empty string.
  bool toProto(::google::protobuf::MessageLite* proto) const;

  // Converts the message to a struct T
  // Returns pointer to T if successful otherwise nullptr.
  // This conversion is zero-copy.
  template <typename T>
  const T* toStruct() const {
    if (getHeader()->type != RAW || sizeof(T) != getBodySize()) {
      return nullptr;
    }
    return reinterpret_cast<const T*>(getBody());
  }

  // Returns the body address of this message.
  const void* getBody() const;

  // Returns the size of the message body in bytes.
  size_t getBodySize() const;

  // Returns the header of this message.
  const Header* getHeader() const;

  // The following 3 functions are used to construct an EaselComm::EaselMessage.
  // Returns the message buffer address of this message.
  void* getMessageBuf() const;

  // Returns the message buffer size in bytes.
  size_t getMessageBufSize() const;

  // Returns the payload
  HardwareBuffer getPayload() const;

  // Returns the message id.
  // Used in Comm::receivePayload to match the message.
  uint64_t getMessageId() const;

  // Returns true if message carries a payload, otherwise false.
  bool hasPayload() const;

 private:
  bool allocMessage(size_t bodySize);

  void attachPayload(const HardwareBuffer& srcBuffer);

  void* getMutableBody();

  Header* getMutableHeader();

  void initializeHeader(int channelId, Type type);

  void* mMessageBuf;
  size_t mMessageBufSize;
  HardwareBuffer mPayload;
  bool mAllocMessage;  // Flag to indicate if mMessageBuf is allocated and owned
                       // by this message.
  uint64_t
      mMessageId;  // Message ID of EaselMessage, used to match DMA transfer.
};
}  // namespace EaselComm2

#endif  // PAINTBOX_EASELCOMM2_MESSAGE
