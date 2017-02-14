#ifndef _GOOGLE_PAINTBOX_TEST_INFRA_LIBPBTIMESSENGER_INCLUDE_EASELMESSENGER_H_
#define _GOOGLE_PAINTBOX_TEST_INFRA_LIBPBTIMESSENGER_INCLUDE_EASELMESSENGER_H_

#include <stdint.h>
#include <array>
#include <mutex>
#include <thread>
#include <vector>
#include <string>

#include "easelcomm.h"

namespace pbti {

typedef int32_t status_t;

/*
 * Message class
 *
 * Defines a message class that can be used to serialize and deserialize data.
 * This class is not thread safe to avoid excessive mutex locking/unlocking.
 * Usually the caller should only need to access a message object in a single
 * thread. If there are multiple threads accessing the message object, the
 * caller must protect the message object from being accessed by multiple
 * threads simultaneously.
 */
class Message {
 public:
    Message();
    virtual ~Message();

    /*
     * Initialize message data. If called twice, the old message data will be
     * freed.
     *
     * capacity is the size of the message data in bytes.
     */
    status_t init(int capacity);

    // Free message data.
    void destroy();

    /*
     * Write to the message. Value will be appended if called twice.
     *
     * Returns:
     *  0:          on success.
     *  -ENOMEM:    if there is no space left in the message to write the new
     *              value.
     */
    status_t writeInt32(int32_t value);
    status_t writeUint32(uint32_t value);
    status_t writeInt64(int64_t value);
    status_t writeFloat(float value);
    status_t writeDouble(double value);
    status_t writeChar(char value);
    status_t writeString(const std::string &values);

    /*
     * Read from the message.
     *
     * Returns:
     *  0:          on success.
     *  -ENODATA:   if there is no more data to read.
     */
    status_t readInt32(int32_t *value);
    status_t readUint32(uint32_t *value);
    status_t readInt64(int64_t *value);
    status_t readFloat(float *value);
    status_t readChar(char *value);
    status_t readString(std::string *values);

    // Reset the message. This will not free message data.
    // It will reset the pointer position to the beginning of the buffer.
    void reset();

    // Returns the byte size of valid data in the message.
    int getSize();

    // Returns the pointer to message data.
    void* getData() const;

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
 * EaselMessenger and EaselMessengerListener can be used for communication
 * between a process on AP and a process on Easel. One process can use
 * EaselMessenger to send messages to the other process and use
 * EaselMessengerListener to receive messages from the other process.
 * EaselMessenger does not serialize and deserialize message data so classes
 * derived from EaselMessenger and EaselMessengerListener should implement
 * specific methods to serialize and deserialize message data. For example,
 * MessengerToPbTiClient/MessengerListenerFromPbTiClient and
 * MessengerToPbTiService/MessengerListenerFromPbTiService implement methods to
 * send messages for paintbox test use cases.
 */

/*
 * EaselMessengerListener class
 *
 * Defines an Easel messenger listener class and callbacks that will be invoked
 * when receiving a message from connected EaselMessenger.
 */
class EaselMessengerListener {
 public:
    virtual ~EaselMessengerListener() {}

    // An opaque DMA buffer handle that can be used to
    // call EaselMessenger::transferDmaBuffer().
    typedef void* DmaBufferHandle;

    /*
     * Invoked when a message is received. message is owned by
     * EaselMessengerListener and should not be deleted by the callback
     * function.
     *
     * message is the received message.
     *
     * Returns:
     *  0:          on success.
     *  Non-zero errors depend on the message.
     */
    virtual status_t onMessage(Message *message) = 0;
};

/*
 * EaselMessenger class
 *
 * Defines an EaselMessenger class that can be used to send messages to
 * a connected EaselMessenger.
 */
class EaselMessenger {
 public:
    EaselMessenger();
    virtual ~EaselMessenger();

    /*
     * Connect to the other EaselMessenger.
     *
     * listener is the listener to receive messages from the other
     * EaselMessenger.
     *
     * Returns:
     *  0:          on success.
     *  -EEXIST:    if it's already connected.
     *  -ENODEV:    if connecting failed due to a serious error.
     */
    virtual status_t connect(EaselMessengerListener &listener) = 0;

    // Disconnect from the other EaselMessenger.
    virtual void disconnect();

    // Listener thread loop to handle receiving messages from the other
    // EaselMessenger.
    void listenerThreadLoop();

    /*
     * Transfer a DMA buffer to a local buffer.
     *
     * handle is the DMA buffer handle received in
     *          EaselMessengerListener::onMessageWithDmaBuffer().
     * dest is a pointer to the destination buffer where the DMA buffer will be
     *          copied to.
     * destSize is the size of the destination buffer. It must be the same as
     *          dmaBufferSize received in
     *          EaselMessengerListener::onMessageWithDmaBuffer().
     *
     * Returns:
     *  0:          on success.
     *  -EINVAL:    if handle is null, dest is null, or destSize doesn't match
     *              the DMA buffer size.
     *  -ENOSYS:    if the low-level driver failed to transfer DMA buffer.
     *  -ENODEV:    if the messenger is not connected.
     */
    status_t transferDmaBuffer(EaselMessengerListener::DmaBufferHandle handle,
                               void* dest, uint32_t destSize);

 protected:
    /*
     * Connect to the other EaselMessenger.
     *
     * listener: the listener to receive messages from the other EaselMessenger.
     * messageSize: the size in bytes of the message data.
     * easelComm: an EaselComm object that can be used to send messages.
     *            easelComm must be already opened and ready to send messages.
     * Returns:
     *  0:          on success.
     *  -EINVAL:    if easelComm is null.
     *  -EEXIST:    if it's already connected.
     *  -ENODEV:    if connecting failed due to a serious error.
     */
    status_t connect(EaselMessengerListener &listener,
                     int messageSize, EaselComm* easelComm);

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
     * Send a message to connected listener. If async is true, this method will
     * not be blocking, i.e. it will send a message and return without waiting
     * for the listener to receive it. If async is false, this method will be
     * blocking, i.e. it will not return until the listener receives and
     * processes it.
     *
     * async is the flag indicating sending the message asynchronously.
     *
     * Returns:
     *  0:          on success.
     *  -EINVAL:    if message is null or the receiver returns -EINVAL for the
     *              message.
     *  -ENODEV:    if messenger is not connected or the receiver returns
     *              -ENODEV for the message.
     *  Other non-zero errors are returned by the receiver depending on the
     *  message.
     */
    status_t sendMessage(Message *message, bool async = false);

    /*
     * Send a message to connected listener with a DMA buffer. This method is
     * blocking. After it returns, DMA buffer transfer is done and the caller
     * still have the ownership of the DMA buffer.
     *
     * Returns:
     *  0:          on success.
     *  -EINVAL:    if message is null or the receiver returns -EINVAL for the
     *              message.
     *  -ENODEV:    if messenger is not connected() or the receiver returns
     *              -ENODEV for the message.
     *  Other non-zero errors are returned by the receiver depending on the
     *  message.
     */
    status_t sendMessageWithDmaBuffer(Message *message, void* dmaBufferSrc,
                                      uint32_t dmaBufferSrcSize);

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
    const int kNumMessages = 16;

    // Defines the data that DmaBufferHandle points to.
    struct DmaBufferInfo {
        // Original EaselMessage received via EaselComm::receiveMessage().
        EaselComm::EaselMessage *easelMessage;
        // A flag indicating if the DMA buffer is transfered.
        bool transferred;
    };

    // Disconnect with mEaselCommLock held.
    void disconnectWithLockHeld();

    /*
     * Return a message with mAvailableMessagesLock held.
     * Returns:
     *  0:          on success.
     * -EINVAL:     if message is null.
     */
    status_t returnMessageLocked(Message *message);

    /*
     * Send a message to connected listener with a DMA buffer if specified. If
     * async is true, this method will not be blocking, i.e. it will send a
     * message and return without waiting for the listener to receive it. If
     * async is false, this method will be blocking, i.e. it will not return
     * until the listener receives and processes it. If a DMA buffer is
     * specified, it cannot be asynchronous.
     *
     * Returns:
     *  0:          on success.
     *  -EINVAL:    if message is null or the receiver returns -EINVAL for the
                    message. Or dmaBufferSrc is not nullptr and async is true.
     *  -ENODEV:    if messenger is not connected or the receiver returns
     *              -ENODEV for the message.
     *  Other non-zero errors are returned by the receiver depending on the
     *  message.
     */
    status_t sendMessageInternal(Message *message, void* dmaBufferSrc,
                                 uint32_t dmaBufferSrcSize, bool async);

    // Protect mAvailableMessages from being accessed simultaneously.
    std::mutex mAvailableMessagesLock;
    /*
     * Messages that are available to get via getEmptyMessage().
     * Messages are created in connect() to avoid repeated new/delete.
     */
    std::vector<Message*> mAvailableMessages;

    // Listener to invoke callbacks when received messages from the connected
    // messenger.
    EaselMessengerListener *mListener;
    std::thread *mListenerThread;

    // Protect mEaselComm from being accessed simultaneously.
    std::mutex mEaselCommLock;
    // Instance of EaselComm object to send and receive messages.
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

template<typename T>
status_t Message::read(T *value) {
    if (value == nullptr) return -EINVAL;
    if (mDataPos + sizeof(T) > mDataSize) {
        return -ENODATA;
    }

    *value = *reinterpret_cast<T*>(mData + mDataPos);
    mDataPos += sizeof(T);

    return 0;
}

template<typename T>
status_t Message::readArray(T *values) {
    if (values == nullptr) {
        return -EINVAL;
    }

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

#define RETURN_ERROR_ON_READ_ERROR(_expr) \
    do { \
        status_t res = (_expr); \
        if (res != 0) { \
            ALOGE("%s: reading message failed: %s (%d)", __FUNCTION__, \
                  strerror(-res), res); \
            return res; \
        } \
    } while (0)

#define RETURN_ON_READ_ERROR(_expr) \
    do { \
        status_t res = (_expr); \
        if (res != 0) { \
            ALOGE("%s: reading message failed: %s (%d)", __FUNCTION__, \
            strerror(-res), res); \
            return; \
        } \
    } while (0)

#define RETURN_ERROR_ON_WRITE_ERROR(_expr) \
    do { \
        status_t res = (_expr); \
        if (res != 0) { \
            returnMessage(message); \
            ALOGE("%s: writing message failed: %s (%d)", __FUNCTION__, \
                  strerror(-res), res); \
            return res; \
        } \
    } while (0)

#define RETURN_ON_WRITE_ERROR(_expr) \
    do { \
        status_t res = (_expr); \
        if (res < 0) { \
            returnMessage(message); \
            ALOGE("%s: writing message failed: %s (%d)", __FUNCTION__, \
                  strerror(-res), res); \
            return; \
        } \
    } while (0)

}  // namespace pbti

#endif // _GOOGLE_PAINTBOX_TEST_INFRA_LIBPBTIMESSENGER_INCLUDE_EASELMESSENGER_H_
