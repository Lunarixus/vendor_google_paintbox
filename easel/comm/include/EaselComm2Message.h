#ifndef PAINTBOX_EASELCOMM2_MESSAGE
#define PAINTBOX_EASELCOMM2_MESSAGE

#include <string>

#include <google/protobuf/message_lite.h>

namespace EaselComm2 {

// Abstraction of device buffers supported in EaselComm2
// for buffer transfering on PCIe.
// Data structure is similar to hidl_memory
struct HardwareBuffer {
  int ionFd;
  size_t size;
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
  };

  // Message header.
  struct Header {
    int channelId;    // Message channel ID.
    Type type;        // Message type.
    bool hasPayload;  // Whether payload buffer is attached.
  };

  Message(int channelId, const std::string& s,
          const HardwareBuffer* payload = nullptr);

  Message(int channelId, const ::google::protobuf::MessageLite& proto,
          const HardwareBuffer* payload = nullptr);

  Message(int channelId, const void* body, size_t size,
          const HardwareBuffer* payload = nullptr);

  Message(void* messageBuf, size_t messageBufSize, int dmaBufFd,
          size_t dmaBufSize, uint64_t messageId);

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

  // The following 4 functions are used to construct an EaselComm::EaselMessage.
  // Returns the message buffer address of this message.
  void* getMessageBuf() const;

  // Returns the message buffer size in bytes.
  size_t getMessageBufSize() const;

  // Returns the payload buffer ion fd.
  int getDmaBufFd() const;

  // Returns the payload buffer size in bytes.
  size_t getDmaBufSize() const;

  // Returns the message id.
  // Used in Comm::receivePayload to match the message.
  uint64_t getMessageId() const;

 private:
  bool allocMessage(size_t bodySize);

  void attachPayload(const HardwareBuffer& srcBuffer);

  void* getMutableBody();

  Header* getMutableHeader();

  void* mMessageBuf;
  size_t mMessageBufSize;
  int mDmaBufFd;
  size_t mDmaBufSize;
  bool mAllocMessage;  // Flag to indicate if mMessageBuf is allocated and owned
                       // by this message.
  uint64_t
      mMessageId;  // Message ID of EaselMessage, used to match DMA transfer.
};
}  // namespace EaselComm2

#endif  // PAINTBOX_EASELCOMM2_MESSAGE
