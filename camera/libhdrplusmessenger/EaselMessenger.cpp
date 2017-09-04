/*
 * Copyright 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// #define LOG_NDEBUG 0
#define LOG_TAG "EaselMessenger"
#include <log/log.h>

#include <stdlib.h>
#include <string>

#include "EaselMessenger.h"

namespace pbcamera {

Message::Message() : mData(nullptr), mDataPos(0), mDataSize(0), mCapacity(0) {
}

Message::~Message() {
    destroy();
}

status_t Message::create(int capacity) {
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
    clear();
    if (mData != nullptr) {
        free(mData);
        mData = nullptr;
        mCapacity = 0;
    }
}

void Message::clear() {
    mDataPos = 0;
    mDataSize = 0;
}

int Message::size() {
    return mDataSize;
}

uint8_t* Message::data() {
    return mData;
}

status_t Message::setData(void *data, size_t size) {
    if (size > mCapacity) return -ENOMEM;

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

status_t Message::writeByte(uint8_t value) {
    return writeUint32(static_cast<uint32_t>(value));
}

status_t Message::writeInt32Vector(const std::vector<int32_t> &values) {
    return writeArrayOrVector(values);
}

status_t Message::writeFloatVector(const std::vector<float> &values) {
    return writeArrayOrVector(values);
}

status_t Message::writeByteVector(const std::vector<uint8_t> &values) {
    // Write the size of the vector.
    status_t res = writeUint32(values.size());
    if (res != 0) return res;

    for (auto value : values) {
        res = writeByte(value);
        if (res != 0) return res;
    }
    return 0;
}

status_t Message::writeString(const std::string &str) {
    // Write string length.
    status_t res = writeUint32(str.size());
    if (res != 0) return res;

    // Write string content.
    if (mDataPos + str.size() > mCapacity) return -ENOMEM;

    memcpy(mData + mDataPos, str.c_str(), str.size());
    mDataPos += str.size();
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

status_t Message::readByte(uint8_t *value) {
    uint32_t i = 0;
    status_t res = readUint32(&i);
    if (res != 0) {
        return res;
    }

    *value = static_cast<uint8_t>(i);
    return 0;
}

template<typename T>
status_t Message::readVector(T *values) {
    if (values == nullptr) return -EINVAL;

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

status_t Message::readInt32Vector(std::vector<int32_t> *values) {
    return readVector(values);
}

status_t Message::readFloatVector(std::vector<float> *values) {
    return readVector(values);
}

status_t Message::readByteVector(std::vector<uint8_t> *values) {
    if (values == nullptr) return -EINVAL;

    // Read number of entries.
    uint32_t numEntries = 0;
    status_t res = readUint32(&numEntries);
    if (res != 0) return res;

    values->resize(numEntries);

    for (size_t i = 0; i < numEntries; i++) {
        res = readByte(&(*values)[i]);
        if (res != 0) return res;
    }

    return 0;
}

status_t Message::readString(std::string *str) {
    if (str == nullptr) return -EINVAL;

    // Read string length
    uint32_t length = 0;
    status_t res = readUint32(&length);
    if (res != 0) return res;

    str->append((const char *)(mData + mDataPos), length);
    mDataPos += length;

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

status_t EaselMessenger::connect(EaselMessengerListener &listener, int maxMessageSize,
        EaselComm *easelComm) {
    if (easelComm == nullptr) return -EINVAL;

    {
        std::lock_guard<std::mutex> lock(mEaselCommLock);

        // Already connected?
        if (mEaselComm != nullptr) return -EEXIST;

        // Initialize messages.
        for (int i = 0; i < kNumMessages; i++) {
            Message *message = new Message();
            status_t res = message->create(maxMessageSize);
            if (res != 0) {
                ALOGE("%s: Creating a message failed: %s (%d).", __FUNCTION__, strerror(-res), res);
                cleanupEaselCommLocked();
                return -ENODEV;
            }
            std::lock_guard<std::mutex> messageLock(mAvailableMessagesLock);
            mAvailableMessages.push_back(message);
        }

        mEaselComm = easelComm;
    }

    {
        std::lock_guard<std::mutex> lock(mListenerLock);

        // Start listener thread.
        mListener = &listener;
        mListenerThread = new std::thread(listenerThreadFunc, this);
    }

    return 0;
}

void EaselMessenger::disconnect() {
    {
        std::lock_guard<std::mutex> lock(mListenerLock);

        // Close listener thread.
        if (mListenerThread != nullptr){
            mListenerThread->join();
            delete mListenerThread;
            mListenerThread = nullptr;
        }

        mListener = nullptr;
    }

    std::lock_guard<std::mutex> lock(mEaselCommLock);
    cleanupEaselCommLocked();
}

void EaselMessenger::cleanupEaselCommLocked() {
    std::lock_guard<std::mutex> messageLock(mAvailableMessagesLock);
    for (auto message : mAvailableMessages) {
        delete message;
    }
    mAvailableMessages.clear();

    mEaselComm = nullptr;
}

status_t EaselMessenger::getEmptyMessage(Message **message) {
    if (message == nullptr) return -EINVAL;

    std::lock_guard<std::mutex> messageLock(mAvailableMessagesLock);
    if (mAvailableMessages.size() == 0) {
        // TODO: wait for it to be available.
        return -ENOENT;
    }

    *message = mAvailableMessages.back();
    (*message)->clear();
    mAvailableMessages.pop_back();

    return 0;
}

status_t EaselMessenger::returnMessage(Message *message) {
    std::lock_guard<std::mutex> messageLock(mAvailableMessagesLock);
    return returnMessageLocked(message);
}

status_t EaselMessenger::returnMessageLocked(Message *message) {
    if (message == nullptr) return -EINVAL;
    mAvailableMessages.push_back(message);
    return 0;
}

status_t EaselMessenger::sendMessage(Message *message, bool async) {
    return sendMessageInternal(message, nullptr, 0, /*dmaBufferSrcFd*/-1, async);
}

status_t EaselMessenger::sendMessageWithDmaBuffer(Message *message, void* dmaBufferSrc,
        uint32_t dmaBufferSrcSize, int dmaBufferSrcFd) {
    return sendMessageInternal(message, dmaBufferSrc, dmaBufferSrcSize, dmaBufferSrcFd,
            /*async*/false);
}

status_t EaselMessenger::sendMessageInternal(Message *message, void* dmaBufferSrc,
            uint32_t dmaBufferSrcSize, int dmaBufferSrcFd, bool async) {
    if (message == nullptr) return -EINVAL;

    std::lock_guard<std::mutex> lock(mEaselCommLock);

    if (async && dmaBufferSrc != nullptr) {
        // Sending a DMA buffer asynchronously is not supported because the caller doesn't know
        // when the DMA buffer transfer completes (i.e. when the caller can safely destroy the
        // buffer being transferred.)
        ALOGE("%s: Sending a DMA buffer asynchronously is not supported.", __FUNCTION__);
        return -EINVAL;
    }

    if (dmaBufferSrc != nullptr && dmaBufferSrcFd != -1) {
        ALOGE("%s: Both dmaBufferSrc and dmaBufferSrcFd are valid.", __FUNCTION__);
        return -EINVAL;
    }

    // Check if it's connected.
    if (mEaselComm == nullptr) return -ENODEV;

    EaselComm::EaselMessage easelMessage = {};
    easelMessage.message_buf = message->data();
    easelMessage.message_buf_size = message->size();
    easelMessage.need_reply = !async;
    easelMessage.dma_buf = dmaBufferSrc;
    easelMessage.dma_buf_size = dmaBufferSrcSize;
    easelMessage.dma_buf_fd = dmaBufferSrcFd;
    if (dmaBufferSrcFd >= 0) {
        easelMessage.dma_buf_type = EASELCOMM_DMA_BUFFER_DMA_BUF;
    } else {
        easelMessage.dma_buf_type = EASELCOMM_DMA_BUFFER_USER;
    }

    status_t res = 0;
    if (async) {
        res = mEaselComm->sendMessage(&easelMessage);
    } else {
        status_t replycode = 0;
        res = mEaselComm->sendMessageReceiveReply(&easelMessage, &replycode, nullptr);
        if (res == 0) {
            res = replycode;
        }
    }

    if (res != 0) {
        ALOGE("%s: sending %s message failed: %s (%d)", __FUNCTION__, async ? "an async" : "a sync",
                strerror(-res), res);
    }

    // Return message to message queue.
    returnMessageLocked(message);

    return res;
}

status_t EaselMessenger::transferDmaBuffer(EaselMessengerListener::DmaBufferHandle handle,
        int32_t dmaBufFd, void* dest, uint32_t bufferSize) {
    if (handle == nullptr) {
        ALOGE("%s: DMA buffer handle is nullptr", __FUNCTION__);
        return -EINVAL;
    }

    if (dmaBufFd < 0 && dest == nullptr) {
        ALOGE("%s: dmaBufFd and dest are both invalid.", __FUNCTION__);
        return -EINVAL;
    }

    DmaBufferInfo *dmaBufferInfo = reinterpret_cast<DmaBufferInfo*>(handle);

    // Source buffer may be smaller than destination buffer.
    if (dmaBufferInfo->easelMessage->dma_buf_size > bufferSize) {
        ALOGE("%s: Source buffer size is %u but destination buffer size is %u", __FUNCTION__,
                (uint32_t)dmaBufferInfo->easelMessage->dma_buf_size, bufferSize);
        return -EINVAL;
    }

    if (dmaBufFd >= 0) {
        dmaBufferInfo->easelMessage->dma_buf_type = EASELCOMM_DMA_BUFFER_DMA_BUF;
    } else {
        dmaBufferInfo->easelMessage->dma_buf_type = EASELCOMM_DMA_BUFFER_USER;
    }

    dmaBufferInfo->easelMessage->dma_buf = dest;
    dmaBufferInfo->easelMessage->dma_buf_fd = dmaBufFd;

    // Mark that DMA buffer is transferred.
    dmaBufferInfo->transferred = true;

    // We don't need to lock mEaselCommLock here because this should be called in mListenerThread
    // and mEaselComm won't be nullptr when mListenerThread is running. Locking mEaselCommLock here
    // may cause deadlock when disconnect() acquired mEaselCommLock and waiting for outstanding
    // DMA buffer transfer to complete.
    status_t res = mEaselComm->receiveDMA(dmaBufferInfo->easelMessage);
    if (res != 0) {
        ALOGE("%s: receiveDMA failed: %s (%d).", __FUNCTION__, strerror(-res), res);
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
            message->setData(easelMessage.message_buf, easelMessage.message_buf_size);

            // TODO: Handle reply message.

            // Calling listener's message callbacks.
            if (easelMessage.dma_buf_size != 0) {
                DmaBufferInfo dmaBufferInfo = {};
                dmaBufferInfo.easelMessage = &easelMessage;
                dmaBufferInfo.transferred = false;

                res = mListener->onMessageWithDmaBuffer(message, &dmaBufferInfo,
                        easelMessage.dma_buf_size);

                // If DMA buffer is not transferred by callback, cancel it.
                if (dmaBufferInfo.transferred == false) {
                    dmaBufferInfo.easelMessage->dma_buf_fd = kInvalidDmaBufFd;
                    dmaBufferInfo.easelMessage->dma_buf = nullptr;
                    mEaselComm->receiveDMA(&easelMessage);
                }
            } else {
                res = mListener->onMessage(message);
            }

            if (easelMessage.need_reply) {
                res = mEaselComm->sendReply(&easelMessage, res, nullptr);
                if (res != 0) {
                    ALOGE("%s: Sending a reply failed: %s (%d).", __FUNCTION__, strerror(-res),
                            res);
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

                {
                    std::lock_guard<std::mutex> lock(mEaselCommLock);
                    mEaselComm = nullptr;
                }

                // Notify the listener that EaselComm has been closed.
                mListener->onEaselCommClosed();
                return;
            }

            // TODO: Handle the error?
            ALOGE("%s: receiveMessage failed: %s (%d).", __FUNCTION__, strerror(-errno), errno);
        }
    }
}

} // namespace pbcamera
