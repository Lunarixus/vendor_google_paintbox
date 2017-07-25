#define LOG_TAG "EaselComm2::Message"

#include "EaselComm2Message.h"

#include "log/log.h"

namespace EaselComm2 {

Message::Message(int channelId, const std::string& s,
                 const HardwareBuffer* payload) {
  size_t stringBufSize = s.size() + 1;
  ALOG_ASSERT(allocMessage(stringBufSize));
  mMessageId = 0;
  auto header = getMutableHeader();
  header->channelId = channelId;
  header->type = STRING;
  char* body = static_cast<char*>(getMutableBody());
  s.copy(body, s.size());
  body[s.size()] = '\0';

  header->hasPayload = false;
  mDmaBufSize = 0;

  if (payload != nullptr) {
    attachPayload(*payload);
  }
}

Message::Message(int channelId, const ::google::protobuf::MessageLite& proto,
                 const HardwareBuffer* payload) {
  size_t size = proto.ByteSize();
  ALOG_ASSERT(allocMessage(size));
  mMessageId = 0;
  auto header = getMutableHeader();
  header->channelId = channelId;
  header->type = PROTO;
  proto.SerializeToArray(getMutableBody(), size);

  header->hasPayload = false;
  mDmaBufSize = 0;

  if (payload != nullptr) {
    attachPayload(*payload);
  }
}

Message::Message(int channelId, const void* body, size_t size,
                 const HardwareBuffer* payload) {
  ALOG_ASSERT(body != nullptr);
  ALOG_ASSERT(allocMessage(size));
  mMessageId = 0;
  auto header = getMutableHeader();
  header->channelId = channelId;
  header->type = RAW;
  memcpy(getMutableBody(), body, size);

  header->hasPayload = false;
  mDmaBufSize = 0;

  if (payload != nullptr) {
    attachPayload(*payload);
  }
}

Message::Message(void* messageBuf, size_t messageBufSize, int dmaBufFd,
                 size_t dmaBufSize, uint64_t messageId) {
  mMessageBuf = messageBuf;
  mMessageBufSize = messageBufSize;
  mDmaBufFd = dmaBufFd;
  mDmaBufSize = dmaBufSize;
  mAllocMessage = false;
  mMessageId = messageId;
}

Message::~Message() {
  if (mAllocMessage) free(mMessageBuf);
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
  header->hasPayload = true;
  header->desc = payload.desc();
  mDmaBufFd = payload.ionFd();
  mDmaBufSize = payload.size();
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

int Message::getDmaBufFd() const { return mDmaBufFd; }

size_t Message::getDmaBufSize() const { return mDmaBufSize; }

uint64_t Message::getMessageId() const { return mMessageId; }
}  // namespace EaselComm2
