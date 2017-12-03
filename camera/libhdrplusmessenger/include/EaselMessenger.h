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
#ifndef EASEL_MESSENGER_H
#define EASEL_MESSENGER_H

#include <array>
#include <mutex>
#include <stdint.h>
#include <thread>
#include <vector>

#include "easelcomm.h"

namespace pbcamera {

typedef int32_t status_t;

/*
 * Message class
 *
 * Defines a message class that can be used to serialize and deserialize data. This class is not
 * thread safe to avoid excessive mutex locking/unlocking. Usually the caller should only need to
 * access a message object in a single thread. If there are multiple threads accessing the message
 * object, the caller must protect the message object from being accessed by multiple threads
 * simultaneously.
 */
class Message {
public:
    Message();
    virtual ~Message();

    /*
     * Initialize message data. If called twice, the old message data will be freed.
     *
     * capacity is the size of the message data in bytes.
     */
    status_t create(int capacity);

    // Free message data.
    void destroy();

    /*
     * Write to the message.
     *
     * Returns:
     *  0:          on succcess.
     *  -ENOMEM:    if there is no space left in the message to write the new value.
     */
    status_t writeInt32(int32_t value);
    status_t writeUint32(uint32_t value);
    status_t writeInt64(int64_t value);
    status_t writeFloat(float value);
    status_t writeDouble(double value);
    status_t writeByte(uint8_t value);
    status_t writeInt32Vector(const std::vector<int32_t> &values);
    status_t writeFloatVector(const std::vector<float> &values);
    status_t writeByteVector(const std::vector<uint8_t> &values);
    status_t writeString(const std::string &str);

    template<size_t SIZE>
    status_t writeFloatArray(const std::array<float, SIZE> &values);

    template<size_t SIZE>
    status_t writeInt32Array(const std::array<int32_t, SIZE> &values);

    template<size_t SIZE>
    status_t writeDoubleArray(const std::array<double, SIZE> &values);

    /*
     * Read from the message.
     *
     * Returns:
     *  0:          on succcess.
     *  -ENODATA:   if there is no more data to read.
     */
    status_t readInt32(int32_t *value);
    status_t readUint32(uint32_t *value);
    status_t readInt64(int64_t *value);
    status_t readFloat(float *value);
    status_t readByte(uint8_t *value);
    status_t readInt32Vector(std::vector<int32_t> *values);
    status_t readFloatVector(std::vector<float> *values);
    status_t readByteVector(std::vector<uint8_t> *values);
    status_t readString(std::string *str);

    template<size_t SIZE>
    status_t readFloatArray(std::array<float, SIZE> *values);

    template<size_t SIZE>
    status_t readInt32Array(std::array<int32_t, SIZE> *values);

    template<size_t SIZE>
    status_t readDoubleArray(std::array<double, SIZE> *values);

    // Clear the message. This will not free message data.
    void clear();

    // Returns the size of valid data in the message.
    int size();

    // Returns the pointer to message data.
    uint8_t* data();

    /*
     * Set message data so it can be deserialized by read functions.
     *
     * data is the source data to copy from.
     * size is the size of data in bytes.
     *
     * Returns:
     *  0:          on success.
     *  -ENOMEM:    if message's capacity is smaller than specified size.
     */
    status_t setData(void *data, size_t size);

private:
    template<typename T>
    status_t write(T value);

    template<typename T>
    status_t writeArrayOrVector(const T &values);

    template<typename T>
    status_t read(T *value);

    template<typename T>
    status_t readVector(T *values);

    template<typename T>
    status_t readArray(T *values);

    uint8_t *mData;

    // The position of the next read or write.
    uint32_t mDataPos;

    // The size of valid data.
    uint32_t mDataSize;

    // The number of bytes allocated for mData.
    uint32_t mCapacity;
};

/*
 * Communication with EaselMessenger and EaselMessengerListener classes.
 *
 * EaselMessenger and EaselMessengerListener can be used for communication between a process on
 * AP and a process on Easel. One process can use EaselMessenger to send messages to the other
 * process and use EaselMessengerListener to receive messages from the other process.
 * EaselMessenger does not serialize and deserialize message data so classes derived from
 * EaselMessenger and EaselMessengerListener should implement specific methods to serialize
 * and deserialize message data. For example,
 * MessengerToHdrPlusClient/MessengerListenerFromHdrPlusClient and
 * MessengerToHdrPlusService/MessengerListenerFromHdrPlusService implement methods to send messages
 * for HDR+ use cases.
 */

/*
 * EaselMessengerListener class
 *
 * Defines an Easel messenger listener class and callbacks that will be invoked when receiving
 * a message from connected EaselMessenger.
 */
class EaselMessengerListener {
public:
    virtual ~EaselMessengerListener() {};

    // An opaque DMA buffer handle that can be used to call EaselMessenger::transferDmaBuffer().
    typedef void* DmaBufferHandle;

    /*
     * Invoked when a message is received. message is owned by by EaselMessengerListener and should
     * not be deleted by the callback function.
     *
     * message is the received message.
     *
     * Returns:
     *  0:          on success.
     *  Non-zero errors depend on the message.
     */
    virtual status_t onMessage(Message *message) = 0;

    /*
     * Invoked when a message with a DMA buffer is received. message is owned by
     * EaselMessengerListener and should not be deleted by the callback function. When this callback
     * is invoked, EaselMessenger::transferDmaBuffer() can be called to transfer the DMA buffer
     * before returning from the callback. After returning from the callback, the DMA buffer handle
     * will be invalid and can no longer be used to transfer the DMA buffer.
     *
     * message is the received message.
     * handle is the DMA buffer handle that can be used to transfer DMA buffer via
     *        EaselMessenger::transferDmaBuffer. The handle will be invalid once returning from the
     *        callback.
     * dmaBufferSize is the size of the DMA buffer to be transferred.
     *
     * Returns:
     *  0:          on success.
     *  Non-zero errors depend on the message.
     */
    virtual status_t onMessageWithDmaBuffer(Message *message, DmaBufferHandle handle,
            uint32_t dmaBufferSize) = 0;

    /*
     * Invoked when EaselComm has been closed. EaselComm may be closed because the other end close
     * the connection or the other end has crashed.
     */
    virtual void onEaselCommClosed() = 0;
};

/*
 * EaselMessenger class
 *
 * Defines an EaselMessenger class that can be used to send messages to connected EaselMessenger.
 */
class EaselMessenger {
public:
    EaselMessenger();
    virtual ~EaselMessenger();

    /*
     * Connect to the other EaselMessenger.
     *
     * listener is the listener to receive messages from the other EaselMessenger.
     *
     * Returns:
     *  0:          on success.
     *  -EEXIST:    if it's already connected.
     *  -ENODEV:    if connecting failed due to a serious error.
     */
    virtual status_t connect(EaselMessengerListener &listener) = 0;

    /*
     * Disconnect from the other EaselMessenger.
     *
     * isErrorState indicates if the other side is in an error state.
     */
    virtual void disconnect(bool isErrorState = false);

    // Listener thread loop to handle receiving messages from the other EaselMessenger.
    void listenerThreadLoop();

    /*
     * Transfer a DMA buffer to a local buffer.
     *
     * handle is the DMA buffer handle received in EaselMessengerListener::onMessageWithDmaBuffer().
     * dmaBufFd is the DMA buffer fd of the ion buffer where the DMA buffer will be copied to. If
     *          the buffer is an ION buffer, dmaBufFd must be valid.
     * dest is a pointer to the destination buffer where the DMA buffer will be copied to.
     * bufferSize is the size of the buffer. It must be the same as dmaBufferSize received
     *          in EaselMessengerListener::onMessageWithDmaBuffer().
     *
     * Returns:
     *  0:          on success.
     *  -EINVAL:    if handle is null, dmaBufFd is negative and dest is nullptr, or bufferSize
     *              doesn't match the DMA buffer size.
     *  -ENOSYS:    if the low-level driver failed to transfer DMA buffer.
     *  -ENODEV:    if the messenger is not connected.
     */
    status_t transferDmaBuffer(EaselMessengerListener::DmaBufferHandle handle, int32_t dmaBufFd,
            void* dest, uint32_t bufferSize);

protected:
    /*
     * Connect to the other EaselMessenger.
     *
     * listener is the listener to receive messages from the other EaselMessenger.
     * messageSize is the size in bytes of the message data.
     * easelComm is an EaselComm object that can be used to send messages. easelComm must
     *              be already opened and ready to send messages.
     * Returns:
     *  0:          on success.
     *  -EINVAL:    if easelComm is null.
     *  -EEXIST:    if it's already connected.
     *  -ENODEV:    if connecting failed due to a serious error.
     */
    status_t connect(EaselMessengerListener &listener, int messageSize, EaselComm* easelComm);

    /*
     * Get an empty message to write data to.
     *
     * message must be returned by sendMessage or returnMessage.
     *
     * Returns:
     *  0:          on success.
     *  -ENOENT:    if there is no empty message available.
     *  -EINVAL:    if message is null.
     */
    status_t getEmptyMessage(Message **message);

    /*
     * Send a message to connected listener. If async is true, this method will not be blocking,
     * i.e. it will send a message and return without waiting for the listener to receive it. If
     * async is false, this method will be blocking, i.e. it will not return until the listener
     * receives and processes it.
     *
     * async is the flag indicating sending the message asynchronously.
     *
     * Returns:
     *  0:          on success.
     *  -EINVAL:    if message is null or the receiver returns -EINVAL for the message. Or
     *              dmaBufferSrc is not nullptr and async is true.
     *  -ENODEV:    if messenger is not connected() or the receiver returns -ENODEV for the message.
     *  Other non-zero errors are returned by the receiver depending on the message.
     */
    status_t sendMessage(Message *message, bool async=false);

    /*
     * Send a message to connected listener with a DMA buffer. This method is blocking. After
     * it returns, DMA buffer transfer is done and the caller still have the ownership of the DMA
     * buffer.
     *
     * message is the message to send.
     * dmaBufferSrc points to the source data to be DMA transferred. This must be nullptr if
     *              dmaBufferSrcFd is valid.
     * dmaBufferSrcSize is size of the source buffer.
     * dmaBufferSrcFd is fd of the source ion buffer. If the buffer is an ION buffer, dmaBufferSrcFd
     *                must be valid.
     *
     * Returns:
     *  0:          on success.
     *  -EINVAL:    if message is null or the receiver returns -EINVAL for the message.
     *  -ENODEV:    if messenger is not connected() or the receiver returns -ENODEV for the message.
     *  Other non-zero errors are returned by the receiver depending on the message.
     */
    status_t sendMessageWithDmaBuffer(Message *message, void* dmaBufferSrc,
            uint32_t dmaBufferSrcSize, int dmaBufferSrcFd);

    /*
     * Return a message without sending it.
     *
     * Returns:
     *  0:          on success.
     * -EINVAL:     if message is null.
     */
    status_t returnMessage(Message *message);

private:
    // Default number of messages.
    const int kNumMessages = 32;

    // Invalid DMA buffer fd.
    static const int32_t kInvalidDmaBufFd = -1;

    // Defines the data that DmaBufferHandle points to.
    struct DmaBufferInfo {
        // Original EaselMessage received via EaselComm::receiveMessage().
        EaselComm::EaselMessage *easelMessage;
        // A flag indicating if the DMA buffer is transfered.
        bool transferred;
    };

    // Clean up Easel comm with mEaselCommLock held.
    void cleanupEaselCommLocked();

    /*
     * Return a message with mAvailableMessagesLock held.
     * Returns:
     *  0:          on success.
     * -EINVAL:     if message is null.
     */
    status_t returnMessageLocked(Message *message);

    /*
     * Send a message to connected listener with a DMA buffer if specified. If async is true, this
     * method will not be blocking, i.e. it will send a message and return without waiting for
     * the listener to receive it. If async is false, this method will be blocking, i.e. it will not
     * return until the listener receives and processes it. If a DMA buffer is specified, it cannot
     * be asynchronous.
     *
     * message is the message to send.
     * dmaBufferSrc points to the source data to be DMA transferred. This must be nullptr if
     *              dmaBufferSrcFd is valid.
     * dmaBufferSrcSize is size of the source buffer.
     * dmaBufferSrcFd is fd of the source ion buffer. If the buffer is an ION buffer, dmaBufferSrcFd
     *                must be valid.
     * async indicates if the message will be sent asynchronously.
     *
     * Returns:
     *  0:          on success.
     *  -EINVAL:    if message is null or the receiver returns -EINVAL for the message. Or
     *              dmaBufferSrc is not nullptr and async is true.
     *  -ENODEV:    if messenger is not connected() or the receiver returns -ENODEV for the message.
     *  Other non-zero errors are returned by the receiver depending on the message.
     */
    status_t sendMessageInternal(Message *message, void* dmaBufferSrc,
            uint32_t dmaBufferSrcSize, int dmaBufferFd, bool async);

    // Protect mAvailableMessages from being accessed simultaneously.
    std::mutex mAvailableMessagesLock;
    /*
     * Messages that are available to get via getEmptyMessage(). Messages are created
     * in connect() to avoid repeated new/delete.
     */
    std::vector<Message*> mAvailableMessages;

    // Listener to invoke callbacks when received messages from the connected messenger.
    std::mutex mListenerLock;
    EaselMessengerListener *mListener; // Protected by mListenerLock.
    std::thread *mListenerThread; // Protected by mListenerLock.

    // Protect mEaselComm from being accessed simultaneously.
    std::mutex mEaselCommLock;
    // Instance of EaselComm object to send and receive messages. Protected by mEaselCommLock.
    EaselComm *mEaselComm;
};

template<typename T>
status_t Message::write(T value) {
    if (mDataPos + sizeof(T) > mCapacity) return -ENOMEM;

    *reinterpret_cast<T*>(mData + mDataPos) = value;
    mDataPos += sizeof(T);
    mDataSize = mDataPos;
    return 0;
}

template<typename T>
status_t Message::writeArrayOrVector(const T &values) {
    // Write number of entries.
    status_t res = writeUint32(values.size());
    if (res != 0) {
        return res;
    }

    // Write values.
    for (size_t i = 0; i < values.size(); i++) {
        res = write(values[i]);
        if (res != 0) {
            return res;
        }
    }

    return 0;
}

template<size_t SIZE>
status_t Message::writeFloatArray(const std::array<float, SIZE> &values) {
    return writeArrayOrVector(values);
}

template<size_t SIZE>
status_t Message::writeInt32Array(const std::array<int32_t, SIZE> &values) {
    return writeArrayOrVector(values);
}

template<size_t SIZE>
status_t Message::writeDoubleArray(const std::array<double, SIZE> &values) {
    return writeArrayOrVector(values);
}

template<typename T>
status_t Message::read(T *value) {
    if (value == nullptr) return -EINVAL;
    if (mDataPos + sizeof(T) > mDataSize) return -ENODATA;

    *value = *reinterpret_cast<T*>(mData + mDataPos);
    mDataPos += sizeof(T);

    return 0;
}

template<typename T>
status_t Message::readArray(T *values) {
    if (values == nullptr) return -EINVAL;

    uint32_t numEntries = 0;

    // Read the number of array entries.
    status_t res = readUint32(&numEntries);
    if (res != 0) {
        return res;
    }

    // Check the number of entries matches.
    if (numEntries != values->size()) {
        return -EINVAL;
    }

    for (size_t i = 0; i < values->size(); i++) {
        res = read(&(*values)[i]);
        if (res != 0) {
            return res;
        }
    }

    return 0;
}

template<size_t SIZE>
status_t Message::readFloatArray(std::array<float, SIZE> *values) {
    return readArray(values);
}

template<size_t SIZE>
status_t Message::readInt32Array(std::array<int32_t, SIZE> *values) {
    return readArray(values);
}

template<size_t SIZE>
status_t Message::readDoubleArray(std::array<double, SIZE> *values) {
    return readArray(values);
}

} // namespace pbcamera

#endif // EASEL_MESSENGER_H
