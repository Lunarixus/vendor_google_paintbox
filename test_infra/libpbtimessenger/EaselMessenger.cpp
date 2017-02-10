// #define LOG_NDEBUG 0
#define LOG_TAG "EaselMessenger"
#include <stdlib.h>
#include "utils/Log.h"

#include "EaselMessenger.h"

namespace pbti {

Message::Message() : mData(nullptr), mDataPos(0), mDataSize(0), mCapacity(0) {
}

Message::~Message() {
    destroy();
}

status_t Message::init(int capacity) {
    if (mData != nullptr) {
        destroy();
    }

    mData = (uint8_t*)malloc(sizeof(uint8_t) * capacity);
    if (mData == nullptr) {
        return -ENOMEM;
    }

    mCapacity = capacity;
    return 0;
}

void Message::destroy() {
    reset();
    if (mData != nullptr) {
        free(mData);
        mData = nullptr;
        mCapacity = 0;
    }
}

void Message::reset() {
    mDataPos = 0;
    mDataSize = 0;
}

int Message::getSize() {
    return mDataSize;
}

void* Message::getData() const {
    return mData;
}

status_t Message::setData(void *data, size_t size) {
    if (size > mCapacity) {
        return -ENOMEM;
    }

    memcpy(mData, data, size);
    mDataPos = 0;
    mDataSize = size;

    return 0;
}

status_t Message::writeInt32(int32_t value) {
    return write(value);
}

status_t Message::writeUint32(uint32_t value) {
    return write(value);
}

status_t Message::writeInt64(int64_t value) {
    return write(value);
}

status_t Message::writeFloat(float value) {
    return write(value);
}

status_t Message::writeDouble(double value) {
    return write(value);
}

status_t Message::writeChar(char value) {
    return write(value);
}

status_t Message::writeString(const std::string &values) {
    // Write the size of the string.
    status_t res = writeUint32(values.size());
    if (res != 0) {
        return res;
    }

    if (mDataPos + values.size() > mCapacity) {
        return -ENOMEM;
    }
    memcpy(mData + mDataPos, values.c_str(), values.size());
    mDataPos += values.size();
    mDataSize = mDataPos;

    return 0;
}

status_t Message::readInt32(int32_t *value) {
    return read(value);
}

status_t Message::readUint32(uint32_t *value) {
    return read(value);
}

status_t Message::readInt64(int64_t *value) {
    return read(value);
}

status_t Message::readFloat(float *value) {
    return read(value);
}

status_t Message::readChar(char *value) {
    return read(value);
}

template<typename T>
status_t Message::readVector(T *values) {
    if (values == nullptr) {
        return -EINVAL;
    }

    uint32_t numEntries = 0;

    // Read the number of array entries.
    status_t res = readUint32(&numEntries);
    if (res != 0) {
        return res;
    }

    values->resize(numEntries);

    for (size_t i = 0; i < numEntries; i++) {
        res = read(&(*values)[i]);
        if (res != 0) {
            return res;
        }
    }

    return 0;
}

status_t Message::readString(std::string *values) {
    if (values == nullptr) {
        return -EINVAL;
    }

    // Read number of entries.
    uint32_t numEntries = 0;
    status_t res = readUint32(&numEntries);
    if (res != 0) {
        return res;
    }

    values->resize(numEntries);

    for (size_t i = 0; i < numEntries; i++) {
        res = readChar(&(*values)[i]);
        if (res != 0) {
            return res;
        }
    }

    return 0;
}

EaselMessenger::EaselMessenger() :
        mListener(nullptr),
        mListenerThread(nullptr),
        mEaselComm(nullptr) {
}

EaselMessenger::~EaselMessenger() {
    disconnect();
}

void listenerThreadFunc(EaselMessenger *messenger) {
    if (messenger == nullptr) return;

    messenger->listenerThreadLoop();
}

status_t EaselMessenger::connect(EaselMessengerListener &listener,
                                 int maxMessageSize, EaselComm *easelComm) {
    if (easelComm == nullptr) {
        return -EINVAL;
    }

    std::lock_guard<std::mutex> lock(mEaselCommLock);

    // Already connected?
    if (mEaselComm != nullptr) {
        return -EEXIST;
    }

    // Initialize messages.
    for (int i = 0; i < kNumMessages; i++) {
        Message *message = new Message();
        status_t res = message->init(maxMessageSize);
        if (res != 0) {
            ALOGE("%s: Creating a message failed: %s (%d).",
                  __FUNCTION__, strerror(-res), res);
            disconnectWithLockHeld();
            return -ENODEV;
        }
        std::lock_guard<std::mutex> messageLock(mAvailableMessagesLock);
        mAvailableMessages.push_back(message);
    }

    mEaselComm = easelComm;

    // Start listener thread.
    mListener = &listener;
    mListenerThread = new std::thread(listenerThreadFunc, this);

    return 0;
}

void EaselMessenger::disconnect() {
    std::lock_guard<std::mutex> lock(mEaselCommLock);
    disconnectWithLockHeld();
}

void EaselMessenger::disconnectWithLockHeld() {
    // Close listener thread.
    if (mListenerThread != nullptr) {
        mListenerThread->join();
        mListenerThread = nullptr;
    }

    std::lock_guard<std::mutex> messageLock(mAvailableMessagesLock);
    for (auto message : mAvailableMessages) {
        delete message;
    }
    mAvailableMessages.clear();

    mEaselComm = nullptr;
    mListener = nullptr;
}

status_t EaselMessenger::getEmptyMessage(Message **message) {
    if (message == nullptr) {
        return -EINVAL;
    }

    std::lock_guard<std::mutex> messageLock(mAvailableMessagesLock);
    if (mAvailableMessages.size() == 0) {
        return -ENOENT;
    }

    *message = mAvailableMessages.back();
    (*message)->reset();
    mAvailableMessages.pop_back();

    return 0;
}

status_t EaselMessenger::returnMessage(Message *message) {
    std::lock_guard<std::mutex> messageLock(mAvailableMessagesLock);
    return returnMessageLocked(message);
}

status_t EaselMessenger::returnMessageLocked(Message *message) {
    if (message == nullptr) {
        return -EINVAL;
    }
    mAvailableMessages.push_back(message);
    return 0;
}

status_t EaselMessenger::sendMessage(Message *message, bool async) {
    return sendMessageInternal(message, nullptr, 0, async);
}

status_t EaselMessenger::sendMessageWithDmaBuffer(Message *message,
         void* dmaBufferSrc, uint32_t dmaBufferSrcSize) {
    return sendMessageInternal(message,
                               dmaBufferSrc, dmaBufferSrcSize, /*async*/false);
}

status_t EaselMessenger::sendMessageInternal(Message *message,
         void* dmaBufferSrc, uint32_t dmaBufferSrcSize, bool async) {
    if (message == nullptr) {
        return -EINVAL;
    }

    std::lock_guard<std::mutex> lock(mEaselCommLock);

    if (async && dmaBufferSrc != nullptr) {
        // Sending a DMA buffer asynchronously is not supported because the
        // caller doesn't know when the DMA buffer transfer completes (i.e.
        // when the caller can safely destroy the buffer being transferred.)
        ALOGE("%s: Sending a DMA buffer asynchronously is not supported.",
              __FUNCTION__);
        return -EINVAL;
    }

    // Check if it's connected.
    if (mEaselComm == nullptr) {
        return -ENODEV;
    }

    EaselComm::EaselMessage easelMessage = {};
    easelMessage.message_buf = message->getData();
    easelMessage.message_buf_size = message->getSize();
    easelMessage.need_reply = !async;
    easelMessage.dma_buf = dmaBufferSrc;
    easelMessage.dma_buf_size = dmaBufferSrcSize;

    status_t res = 0;
    if (async) {
        res = mEaselComm->sendMessage(&easelMessage);
    } else {
        status_t replycode = 0;
        res = mEaselComm->sendMessageReceiveReply(&easelMessage,
                                                  &replycode, nullptr);
        if (res == 0) {
            res = replycode;
        }
    }

    if (res != 0) {
        ALOGE("%s: sending %s message failed: %s (%d)", __FUNCTION__,
              async ? "an async" : "a sync", strerror(-res), res);
    }

    // Return message to message queue.
    returnMessageLocked(message);

    return res;
}

status_t EaselMessenger::transferDmaBuffer(
  EaselMessengerListener::DmaBufferHandle handle, void* dest,
  uint32_t destSize) {
    if (handle == nullptr || dest == nullptr) {
        return -EINVAL;
    }

    DmaBufferInfo *dmaBufferInfo = reinterpret_cast<DmaBufferInfo*>(handle);

    if (dmaBufferInfo->easelMessage->dma_buf_size != destSize) {
        ALOGE("%s: Expecting buffer size %u but destSize is %u", __FUNCTION__,
                (uint32_t)dmaBufferInfo->easelMessage->dma_buf_size, destSize);
        return -EINVAL;
    }

    dmaBufferInfo->easelMessage->dma_buf = dest;

    // Mark that DMA buffer is transferred.
    dmaBufferInfo->transferred = true;

    // We don't need to lock mEaselCommLock here because this should be called
    // in mListenerThread and mEaselComm won't be nullptr when mListenerThread
    // is running. Locking mEaselCommLock here may cause deadlock when
    // disconnect() acquired mEaselCommLock and waiting for outstanding
    // DMA buffer transfer to complete.
    status_t res = mEaselComm->receiveDMA(dmaBufferInfo->easelMessage);
    if (res != 0) {
        ALOGE("%s: receiveDMA failed: %s (%d).",
              __FUNCTION__, strerror(-res), res);
        return -ENOSYS;
    }

    return 0;
}

void EaselMessenger::listenerThreadLoop() {
    status_t res = 0;
    Message *message = nullptr;

    while (1) {
        // Wait for next message.
        EaselComm::EaselMessage easelMessage = {};
        res = mEaselComm->receiveMessage(&easelMessage);
        if (res == 0) {
            res = getEmptyMessage(&message);
            if (res != 0) {
                if (easelMessage.dma_buf_size) {
                    // Cancel DMA
                    mEaselComm->receiveDMA(&easelMessage);
                }
                if (easelMessage.need_reply) {
                    mEaselComm->sendReply(&easelMessage, res, nullptr);
                }

                // Free easel message buffer.
                free(easelMessage.message_buf);
                easelMessage.message_buf = nullptr;
                continue;
            }

            // Set message data so it can be deserialized in onMessage().
            message->setData(easelMessage.message_buf,
                             easelMessage.message_buf_size);

            // Calling listener's message callbacks.
            res = mListener->onMessage(message);

            if (easelMessage.need_reply) {
                res = mEaselComm->sendReply(&easelMessage, res, nullptr);
                if (res != 0) {
                    ALOGE("%s: Sending a reply failed: %s (%d).",
                          __FUNCTION__, strerror(-res), res);
                }
            }

            // Free easel message buffer.
            free(easelMessage.message_buf);
            easelMessage.message_buf = nullptr;

            // Return message to message queue.
            returnMessage(message);
        } else {
            if (errno == ESHUTDOWN) {
                ALOGD("%s: EaselComm has shut down.", __FUNCTION__);
                return;
            }

            // Other errors
            ALOGE("%s: receiveMessage failed: %s (%d).",
                  __FUNCTION__, strerror(-errno), errno);
        }
    }
}

}  // namespace pbti
