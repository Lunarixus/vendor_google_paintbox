#include "EaselComm2Impl.h"

namespace EaselComm2 {

namespace {
void ConvertMessageToEaselMessage(const Message& message,
                                  EaselComm::EaselMessage* easelMessage) {
  easelMessage->message_buf = message.getMessageBuf();
  easelMessage->message_buf_size = message.getMessageBufSize();
  easelMessage->dma_buf = nullptr;
  easelMessage->dma_buf_size = message.getDmaBufSize();
  easelMessage->dma_buf_fd = message.getDmaBufFd();
  easelMessage->dma_buf_type = EASELCOMM_DMA_BUFFER_DMA_BUF;
}
}  // namespace

CommImpl::CommImpl(Mode mode) {
  if (mode == Mode::CLIENT) {
    mComm = std::make_unique<EaselCommClient>();
  } else {
    mComm = std::make_unique<EaselCommServer>();
  }
}

CommImpl::~CommImpl() { mComm->close(); }

int CommImpl::open(EaselService service_id, long timeout_ms) {
  return mComm->open(service_id, timeout_ms);
}

int CommImpl::open(EaselService service_id) {
  return open(service_id, DEFAULT_OPEN_TIMEOUT_MS);
}

void CommImpl::close() { mComm->close(); }

void CommImpl::registerHandler(int channelId, Handler handler) {
  std::lock_guard<std::mutex> lock(mHandlerMapMutex);
  mHandlerMap[channelId] = handler;
}

int CommImpl::startReceiving() {
  return mComm->startMessageHandlerThread([&](EaselComm::EaselMessage* msg) {
    Message message(msg->message_buf, msg->message_buf_size, msg->dma_buf_fd,
                    msg->dma_buf_size, msg->message_id);
    std::lock_guard<std::mutex> lock(mHandlerMapMutex);
    auto handler = mHandlerMap[message.getHeader()->channelId];
    handler(message);
  });
}

void CommImpl::joinReceiving() { return mComm->joinMessageHandlerThread(); }

int CommImpl::receivePayload(const Message& message, HardwareBuffer* buffer) {
  if (buffer == nullptr) return -EINVAL;
  auto srcDesc = message.getHeader()->desc;
  auto destDesc = buffer->desc();

  // Assumes src and dest desc are equal.
  // TODO(cjluo): Need to handle the case when strides are not matching.
  if (srcDesc != destDesc) return -EINVAL;

  EaselComm::EaselMessage easelMessage;
  easelMessage.message_id = message.getMessageId();
  easelMessage.dma_buf = nullptr;
  easelMessage.dma_buf_fd = buffer->ionFd();
  easelMessage.dma_buf_type = EASELCOMM_DMA_BUFFER_DMA_BUF;
  easelMessage.dma_buf_size = message.getDmaBufSize();
  return mComm->receiveDMA(&easelMessage);
}

int CommImpl::send(int channelId, const void* body, size_t body_size,
                   const HardwareBuffer* payload) {
  Message message(channelId, body, body_size, payload);
  EaselComm::EaselMessage easelMessage;
  ConvertMessageToEaselMessage(message, &easelMessage);
  return mComm->sendMessage(&easelMessage);
}

int CommImpl::send(int channelId, const std::string& s,
                   const HardwareBuffer* payload) {
  Message message(channelId, s, payload);
  EaselComm::EaselMessage easelMessage;
  ConvertMessageToEaselMessage(message, &easelMessage);
  return mComm->sendMessage(&easelMessage);
}

int CommImpl::send(int channelId, const ::google::protobuf::MessageLite& proto,
                   const HardwareBuffer* payload) {
  Message message(channelId, proto, payload);
  EaselComm::EaselMessage easelMessage;
  ConvertMessageToEaselMessage(message, &easelMessage);
  easelMessage.need_reply = true;
  return mComm->sendMessage(&easelMessage);
}

}  // namespace EaselComm2
