#define LOG_TAG "EaselComm2::Message"

#include "EaselComm2Message.h"

#include "log/log.h"

#include <fstream>

namespace EaselComm2 {

HardwareBuffer::HardwareBuffer() : vaddr(nullptr), ionFd(-1), size(0), id(0) {}
HardwareBuffer::HardwareBuffer(void* vaddr, size_t size, int id)
    : vaddr(vaddr), ionFd(-1), size(size), id(id) {}
HardwareBuffer::HardwareBuffer(int ionFd, size_t size, int id)
    : vaddr(nullptr), ionFd(ionFd), size(size), id(id) {}

bool HardwareBuffer::isIonBuffer() { return vaddr == nullptr && ionFd > 0; }

void* HardwareBuffer::loadFile(const std::string& filePath) {
  if (vaddr != nullptr || size != 0) {
    ALOGE("%s: buffer not empty!", __FUNCTION__);
    return nullptr;
  }

  std::ifstream input(filePath, std::ifstream::binary);
  if (!input.is_open()) {
    ALOGE("%s: cannot open file %s!", __FUNCTION__, filePath.c_str());
    return nullptr;
  }

  input.seekg(0, input.end);
  size_t fileSize = input.tellg();
  input.seekg(0);
  void* buffer = malloc(fileSize);
  if (buffer == nullptr) {
    ALOGE("%s: cannot allocate buffer", __FUNCTION__);
    return nullptr;
  }

  input.read(static_cast<char*>(buffer), fileSize);

  vaddr = buffer;
  size = fileSize;
  ionFd = -1;
  input.close();
  return buffer;
}

int HardwareBuffer::saveFile(const std::string& filePath) {
  if (isIonBuffer()) return -EINVAL;
  std::ofstream output(filePath, std::ofstream::binary);
  if (!output.is_open()) return -ENOENT;
  output.write(static_cast<char*>(vaddr), size);
  output.close();
  return 0;
}

Message::Message(int channelId, const std::string& s,
                 const HardwareBuffer* payload) {
  size_t stringBufSize = s.size() + 1;
  ALOG_ASSERT(allocMessage(stringBufSize));
  mMessageId = 0;
  initializeHeader(channelId, STRING);
  char* body = static_cast<char*>(getMutableBody());
  s.copy(body, s.size());
  body[s.size()] = '\0';

  if (payload != nullptr) {
    attachPayload(*payload);
  }
}

Message::Message(int channelId, const ::google::protobuf::MessageLite& proto,
                 const HardwareBuffer* payload) {
  size_t size = proto.ByteSize();
  ALOG_ASSERT(allocMessage(size));
  mMessageId = 0;
  initializeHeader(channelId, PROTO);
  proto.SerializeToArray(getMutableBody(), size);

  if (payload != nullptr) {
    attachPayload(*payload);
  }
}

Message::Message(int channelId, const void* body, size_t size,
                 const HardwareBuffer* payload) {
  ALOG_ASSERT(body != nullptr);
  ALOG_ASSERT(allocMessage(size));
  mMessageId = 0;
  initializeHeader(channelId, RAW);
  memcpy(getMutableBody(), body, size);

  if (payload != nullptr) {
    attachPayload(*payload);
  }
}

Message::Message(int channelId, const HardwareBuffer* payload) {
  ALOG_ASSERT(allocMessage(0));
  mMessageId = 0;
  initializeHeader(channelId, PING);

  if (payload != nullptr) {
    attachPayload(*payload);
  }
}

Message::Message(void* messageBuf, size_t messageBufSize, size_t dmaBufSize,
                 uint64_t messageId) {
  mMessageBuf = messageBuf;
  mMessageBufSize = messageBufSize;
  mPayload.size = dmaBufSize;
  mPayload.id = getHeader()->payloadId;
  mAllocMessage = false;
  mMessageId = messageId;
}

Message::~Message() {
  if (mAllocMessage) free(mMessageBuf);
}

void Message::initializeHeader(int channelId, Type type) {
  auto header = getMutableHeader();
  header->channelId = channelId;
  header->type = type;
  header->payloadId = 0;
}

std::string Message::toString() const {
  if (getHeader()->type != STRING) {
    ALOGE("%s failed", __FUNCTION__);
    return "";
  }
  const char* body = static_cast<const char*>(getBody());
  if (body[getBodySize() - 1] != '\0') return "";
  std::string s = std::string(body);
  if (s.size() + 1 != getBodySize()) return "";
  return s;
}

bool Message::toProto(::google::protobuf::MessageLite* proto) const {
  if (getHeader()->type != PROTO || proto == nullptr) {
    ALOGE("%s failed", __FUNCTION__);
    return false;
  }
  return proto->ParseFromArray(getBody(), getBodySize());
}

void Message::attachPayload(const HardwareBuffer& payload) {
  auto header = getMutableHeader();
  header->payloadId = payload.id;
  mPayload = payload;
}

bool Message::allocMessage(size_t bodySize) {
  size_t size = sizeof(Header) + bodySize;
  mMessageBuf = malloc(sizeof(Header) + bodySize);
  if (mMessageBuf != nullptr) {
    mMessageBufSize = size;
    mAllocMessage = true;
    return true;
  }
  mAllocMessage = false;
  return false;
}

const Message::Header* Message::getHeader() const {
  return reinterpret_cast<Header*>(mMessageBuf);
}

Message::Header* Message::getMutableHeader() {
  return reinterpret_cast<Header*>(mMessageBuf);
}

const void* Message::getBody() const {
  return static_cast<char*>(mMessageBuf) + sizeof(Header);
}

void* Message::getMutableBody() {
  return static_cast<char*>(mMessageBuf) + sizeof(Header);
}

size_t Message::getBodySize() const { return mMessageBufSize - sizeof(Header); }

void* Message::getMessageBuf() const { return mMessageBuf; }

size_t Message::getMessageBufSize() const { return mMessageBufSize; }

HardwareBuffer Message::getPayload() const { return mPayload; }

uint64_t Message::getMessageId() const { return mMessageId; }

bool Message::hasPayload() const { return mPayload.size > 0; }
}  // namespace EaselComm2
