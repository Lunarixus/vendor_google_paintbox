#define LOG_TAG "EaselComm2"

#include "EaselComm2Impl.h"

#include "android-base/logging.h"

namespace EaselComm2 {

namespace {
void ConvertMessageToEaselMessage(const Message& message,
                                  EaselComm::EaselMessage* easelMessage) {
  easelMessage->message_buf = message.getMessageBuf();
  easelMessage->message_buf_size = message.getMessageBufSize();
  auto payload = message.getPayload();
  easelMessage->dma_buf = payload.vaddr;
  easelMessage->dma_buf_fd = payload.ionFd;
  easelMessage->dma_buf_size = payload.size;
  easelMessage->dma_buf_type = payload.isIonBuffer()
                                   ? EASELCOMM_DMA_BUFFER_DMA_BUF
                                   : EASELCOMM_DMA_BUFFER_USER;
}

const int kMsToUs = 1000;
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

void CommImpl::openPersistent(EaselService service_id, int retryMs,
                              bool logging) {
  while (true) {
    // open the channel with infinite timeout.
    int res = open(service_id, -1);
    if (logging) {
      LOG(INFO) << __FUNCTION__ << " open channel " << service_id << ", error "
                << res;
    }
    if (res == 0) {
      startReceiving();
      joinReceiving();
    }
    close();
    if (logging) {
      LOG(WARNING) << __FUNCTION__ << " channel " << service_id
                   << " down, reopening...";
    };

    if (res != 0) {
      usleep(retryMs * kMsToUs);
    }
  }
}

void CommImpl::close() { mComm->close(); }

void CommImpl::registerHandler(int channelId, Handler handler) {
  std::lock_guard<std::mutex> lock(mHandlerMapMutex);
  mHandlerMap[channelId] = handler;
}

int CommImpl::startReceiving() {
  return mComm->startMessageHandlerThread([&](EaselComm::EaselMessage* msg) {
    Message message(msg->message_buf, msg->message_buf_size, msg->dma_buf_size,
                    msg->message_id);
    std::lock_guard<std::mutex> lock(mHandlerMapMutex);
    auto& handler = mHandlerMap[message.getHeader()->channelId];
    handler(message);
  });
}

void CommImpl::joinReceiving() { return mComm->joinMessageHandlerThread(); }

int CommImpl::receivePayload(const Message& message, HardwareBuffer* buffer) {
  if (buffer == nullptr) return -EINVAL;
  size_t srcSize = message.getPayload().size;
  size_t destSize = buffer->size;

  if (srcSize != destSize) return -EINVAL;

  buffer->id = message.getHeader()->payloadId;

  EaselComm::EaselMessage easelMessage;
  easelMessage.message_id = message.getMessageId();
  easelMessage.dma_buf = buffer->vaddr;
  easelMessage.dma_buf_fd = buffer->ionFd;
  easelMessage.dma_buf_type = buffer->isIonBuffer()
                                  ? EASELCOMM_DMA_BUFFER_DMA_BUF
                                  : EASELCOMM_DMA_BUFFER_USER;
  easelMessage.dma_buf_size = buffer->size;
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
  return mComm->sendMessage(&easelMessage);
}

int CommImpl::send(int channelId, const std::vector<HardwareBuffer>& buffers,
                   int* lastId) {
  for (auto& buffer : buffers) {
    Message message(channelId, buffer);
    EaselComm::EaselMessage easelMessage;
    ConvertMessageToEaselMessage(message, &easelMessage);
    int ret = mComm->sendMessage(&easelMessage);
    if (ret != 0) {
      return ret;
    } else if (lastId != nullptr) {
      *lastId = buffer.id;
    }
  }
  return 0;
}

}  // namespace EaselComm2
