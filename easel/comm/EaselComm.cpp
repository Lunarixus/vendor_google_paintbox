/*
 * Android/Easel message-passing and DMA communication via the easelcomm
 * kernel driver.
 *
 * Header file easelcomm.h contains documentation on the public APIs below.
 */

#define LOG_TAG "EaselComm"

#include "easelcomm.h"
#include "uapi/linux/google-easel-comm.h"

#include <cstdint>
#include <string>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <log/log.h>

#undef PROFILE_DMA

namespace {
// Device file path
static const char *kEaselCommDevPathClient = "/dev/easelcomm-client";
static const char *kEaselCommDevPathServer = "/dev/easelcomm-server";
static const useconds_t kOpenPollIntervalUs = 1000;  // Poll interval 1 ms

enum {
    KBUF_FILL_UNUSED,
    KBUF_FILL_MSG,
    KBUF_FILL_DMA,
};

static bool is_easelcomm_client() {
    struct stat buffer;
    return (stat(kEaselCommDevPathClient, &buffer) == 0);
}

static std::string get_env(const char* env_name) {
  char* env_value = std::getenv(env_name);
  return (env_value == nullptr) ? "" : std::string(env_value);
}

static bool is_server_logging_to_logcat() {
    std::string dest = get_env("LOG_DEST");
    // Same logic as getLogDest() in LogBufferEasel.cpp
    return !((dest == "CONSOLE") || (dest == "FILE"));
}

static void fill_kbuf(easelcomm_kbuf_desc *buf_desc,
                      easelcomm_msgid_t message_id,
                      const EaselComm::EaselMessage *msg,
                      int fill_type) {

    // message_id should be passed to kbuf_desc no matter it is a DMA or MSG
    // transfer or just to discard DMA tranfser
    buf_desc->message_id = message_id;
    // Assign default timeout to infinite
    buf_desc->wait.timeout_ms = -1;

    if (msg == nullptr || fill_type == KBUF_FILL_UNUSED) {
        buf_desc->buf = nullptr;
        buf_desc->dma_buf_fd = -1;
        buf_desc->buf_type = EASELCOMM_DMA_BUFFER_UNUSED;
        buf_desc->buf_size = 0;
        return;
    }

    // Assign timeout
    buf_desc->wait.timeout_ms = msg->timeout_ms;

    switch (fill_type) {
        case KBUF_FILL_MSG:
            buf_desc->buf = msg->message_buf;
            buf_desc->dma_buf_fd = msg->dma_buf_fd;
            buf_desc->buf_type = msg->dma_buf_type;
            buf_desc->buf_size = msg->message_buf_size;
            break;
        case KBUF_FILL_DMA:
            buf_desc->buf = msg->dma_buf;
            buf_desc->dma_buf_fd = msg->dma_buf_fd;
            buf_desc->buf_type = msg->dma_buf_type;
            buf_desc->buf_size = msg->dma_buf_size;
            break;
    }
}

/*
 * If ALOG* is called inside sendAMessage(), on Easel side it might be calling
 * sendAMessage() again by liblog, creating an infinite loop.
 * Only print ALOG* on the client side or when the server is not sending the
 * log back to AP logcat.
 */
static bool is_alog_ok(void) {
    return is_easelcomm_client() || !is_server_logging_to_logcat();
}

/*
 * Helper for sending a message, called for all APIs that send a message
 * (sendMessage, sendMessageReceiveReply, sendReply).
 *
 * fd is the file descriptor from the EaselComm object opened for the easelcomm
 * device.
 *
 * kmsg_desc is the kernel message descriptor filled out by the caller, sent
 * to the kernel via the SENDMSG ioctl and updated by the ioctl (to add the
 * assigned message_id).
 *
 * msg is the userspace-layer EaselMessage, which has the local buffer
 * pointers for the message data and DMA source buffer (if any).  Can be NULL
 * if there is no message supplied by the caller (that is, this is a reply with
 * a replycode only, no accompanying message, which gets sent as a default
 * message that includes the replycode in the kernel-layer descriptor).
 *
 * Returns after the DMA transfer is complete, if a DMA transfer is requested,
 * else returns once the message is dispatched to the remote.
 *
 * Returns zero for success or a negative errno value for failure.
 * In all cases the kernel copy of the outgoing message is freed.
 */
static int sendAMessage(int fd, struct easelcomm_kmsg_desc *kmsg_desc,
                        const EaselComm::EaselMessage *msg)
{
    easelcomm_kbuf_desc buf_desc;

    /*
     * Send the kernel message descriptor, which starts the outgoing message,
     * and read the updated descriptor with the assigned message ID.
     */
    if (ioctl(fd, EASELCOMM_IOC_SENDMSG, kmsg_desc) == -1) {
        int err_saved = errno;
        if (is_alog_ok()) {
            ALOGE("%s: SENDMSG failed (%d)", __FUNCTION__, err_saved);
        }
        return -err_saved;
    }

    /*
     * Fill out a kernel buffer descriptor for the message data and send
     * to the kernel.  Note that this must happen even if the message
     * buffer size is zero and even if no EaselMessage is supplied (to
     * sendReply).
     */
    if (msg) {
        fill_kbuf(&buf_desc, kmsg_desc->message_id, msg, KBUF_FILL_MSG);
    } else {
        fill_kbuf(&buf_desc, kmsg_desc->message_id, nullptr, KBUF_FILL_UNUSED);
    }
    if (ioctl(fd, EASELCOMM_IOC_WRITEDATA, &buf_desc) == -1) {
        int err_saved = errno;
        if (is_alog_ok()) {
            ALOGE("%s: WRITEDATA failed (%d)", __FUNCTION__, err_saved);
        }
        return -err_saved;
    }

    /*
     * If the message includes a DMA transfer then send the source DMA buffer
     * descriptor.  This initiates the DMA transfer once the remote side is
     * also ready with its destination DMA buffer descriptor (or the remote
     * may discard the DMA transfer, in which case the transfer does not occur).
     * A successful call returns once the DMA transfer is completed.
     */
    if (msg && msg->dma_buf_size) {
        fill_kbuf(&buf_desc, kmsg_desc->message_id, msg, KBUF_FILL_DMA);

        if (ioctl(fd, EASELCOMM_IOC_SENDDMA, &buf_desc) == -1) {
            int err_saved = errno;
            if (is_alog_ok()) {
                ALOGE("%s: SENDDMA failed (%d)", __FUNCTION__, err_saved);
            }
            return -err_saved;
        }
    }

    return 0;
}

static const size_t kHandshakeSignalLen = 10;
static const int kHandshakeSeqNum = 3;
const char *handshakeSeq[kHandshakeSeqNum] = {
    "SYN",
    "SYN-ACK",
    "ACK",
};

static int composeHandshake(EaselComm::EaselMessage *msg, int seq) {
    assert(msg != nullptr);
    assert((seq >= 0) && (seq < kHandshakeSeqNum));

    msg->message_buf = (void *)handshakeSeq[seq];
    msg->message_buf_size = kHandshakeSignalLen;
    msg->dma_buf = nullptr;
    return 0;
}

static int verifyHandshake(EaselComm::EaselMessage *msg, int seq) {
    int ret = 0;

    assert(msg != nullptr);
    assert((seq >= 0) && (seq < kHandshakeSeqNum));

    if (msg->message_buf_size < kHandshakeSignalLen) {
        free(msg->message_buf);
        return -EINVAL;
    }

    if (strcmp((const char *)msg->message_buf, handshakeSeq[seq]) != 0) {
        ret = -EINVAL;
    }

    free(msg->message_buf);
    return ret;
}

}  // anonymous namespace


EaselComm::EaselComm() {
    mEaselCommFd = -1;
    mClosed = true;
    pthread_rwlock_init(&mFdRwlock, nullptr);
}

EaselComm::~EaselComm() {
    close();
    pthread_rwlock_destroy(&mFdRwlock);
}

// Send a message without waiting for a reply.
int EaselComm::sendMessage(const EaselMessage *msg) {
    struct easelcomm_kmsg_desc kmsg_desc;
    int ret = 0;

    kmsg_desc.message_size = msg->message_buf_size;
    kmsg_desc.dma_buf_size = msg->dma_buf_size;
    kmsg_desc.message_id = 0;
    kmsg_desc.need_reply = false;
    kmsg_desc.in_reply_to = 0;
    kmsg_desc.replycode = 0;

    pthread_rwlock_rdlock(&mFdRwlock);   // Acquire rwlock as a reader
    ret = sendAMessage(mEaselCommFd, &kmsg_desc, msg);
    pthread_rwlock_unlock(&mFdRwlock);
    return ret;
}

// Send a message and wait for a reply.
int EaselComm::sendMessageReceiveReply(
    const EaselMessage *msg, int *replycode, EaselMessage *reply) {
    struct easelcomm_kmsg_desc kmsg_desc;
    struct easelcomm_kbuf_desc buf_desc;
    int ret;

    /* Cleanup any harmful junk in caller's arg in case we bail early. */
    if (reply) {
        reply->message_buf = nullptr;
        reply->message_buf_size = 0;
        reply->dma_buf = nullptr;
        reply->dma_buf_size = 0;
    }

    kmsg_desc.message_size = msg->message_buf_size;
    kmsg_desc.dma_buf_size = msg->dma_buf_size;
    kmsg_desc.message_id = 0;
    kmsg_desc.need_reply = msg->need_reply;
    assert(msg->need_reply == true);
    kmsg_desc.in_reply_to = 0;
    // Wait for timeout_ms
    kmsg_desc.wait.timeout_ms = msg->timeout_ms;

    pthread_rwlock_rdlock(&mFdRwlock);   // Acquire rwlock as a reader
    ret = sendAMessage(mEaselCommFd, &kmsg_desc, msg);
    pthread_rwlock_unlock(&mFdRwlock);
    if (ret)
        return ret;

    /*
     * Wait for and return the reply message descriptor.  If the remote
     * sendReply() does not include a message then a skeleton message with
     * zero-lenth message data and no DMA transfer is returned, with the
     * remote's replycode.
     */
    if (ioctl(mEaselCommFd, EASELCOMM_IOC_WAITREPLY, &kmsg_desc) == -1) {
        ALOGE("%s: WAITREPLY failed (%d)", __FUNCTION__, errno);
        return -errno;
    }

    // Acquire rwlock as a reader; WAITREPLY does not need to acquire rdlock
    pthread_rwlock_rdlock(&mFdRwlock);
    /*
     * Caller can omit the reply EaselMessage parameter.  Normally this is
     * expected only if the caller knows no reply message is sent, only a
     * replycode.  But just in case, if the caller doesn't supply a reply
     * message param and a message is sent with a DMA transfer requested,
     * discard the DMA transfer so both local and remote clean up their
     * copies of the in-progress message.  If either message data or DMA
     * data is sent by the remote and is being discarded, return -EIO
     * assuming this is an error condition.
     */
    if (reply) {
        reply->message_buf_size = kmsg_desc.message_size;
        reply->dma_buf_size = kmsg_desc.dma_buf_size;
        reply->message_id = kmsg_desc.message_id;
        reply->need_reply = kmsg_desc.need_reply;
        assert(reply->need_reply == false);

        if (reply->message_buf_size) {
            reply->message_buf = malloc(reply->message_buf_size);
            if (reply->message_buf == nullptr) {
                ret = -errno;
                pthread_rwlock_unlock(&mFdRwlock);
                return ret;
            }

            fill_kbuf(&buf_desc, reply->message_id, reply, KBUF_FILL_MSG);


            if (ioctl(mEaselCommFd, EASELCOMM_IOC_READDATA, &buf_desc) == -1) {
                ALOGE("%s: READDATA failed (%d)", __FUNCTION__, errno);
                free(reply->message_buf);
                reply->message_buf = nullptr;
                ret = -errno;
            }
        }
    } else {
        /*
         * No reply message param.  Discard reply message data and DMA
         * transfer, return -EIO if either type of data was discarded, assuming
         * this is an error in the application logic.
         */
        if (kmsg_desc.message_size || kmsg_desc.dma_buf_size)
            ret = -EIO;
        fill_kbuf(&buf_desc, kmsg_desc.message_id, nullptr, KBUF_FILL_UNUSED);

        if (ioctl(mEaselCommFd, EASELCOMM_IOC_READDATA, &buf_desc) == -1) {
            ALOGE("%s: READDATA failed (%d)", __FUNCTION__, errno);
            pthread_rwlock_unlock(&mFdRwlock);
            return -errno;
        }
        if (kmsg_desc.dma_buf_size) {
            if (ioctl(mEaselCommFd, EASELCOMM_IOC_RECVDMA, &buf_desc) == -1) {
                ALOGE("%s: RECVDMA failed (%d)", __FUNCTION__, errno);
                pthread_rwlock_unlock(&mFdRwlock);
                return -errno;
            }
        }
    }
    pthread_rwlock_unlock(&mFdRwlock);

    *replycode = kmsg_desc.replycode;
    return ret;
}

// Wait for and return next incoming Easel Message.
int EaselComm::receiveMessage(EaselMessage *msg) {
    struct easelcomm_kmsg_desc kmsg_desc;
    struct easelcomm_kbuf_desc buf_desc;
    int ret = 0;

    /* Cleanup any harmful junk in caller's arg in case we bail early */
    msg->message_buf = nullptr;
    msg->message_buf_size = 0;
    msg->dma_buf = nullptr;
    msg->dma_buf_size = 0;

    // Wait for timeout_ms
    kmsg_desc.wait.timeout_ms = msg->timeout_ms;
    if (ioctl(mEaselCommFd, EASELCOMM_IOC_WAITMSG, &kmsg_desc) == -1) {
        /*
         * If close() method was called by another thread in parallel the
         * fd may be invalid.  Treat the same as evicting a WAITMSG waiter and
         * return "connection shut down" status.
         */
        if (errno == EBADF) {
                errno = ESHUTDOWN;
        }

        if (errno != ESHUTDOWN) {
            ALOGE("%s: WAITMSG failed (%d)", __FUNCTION__, errno);
        }
        return -errno;
    }

    msg->message_buf_size = kmsg_desc.message_size;
    msg->dma_buf_size = kmsg_desc.dma_buf_size;
    msg->message_id = kmsg_desc.message_id;
    msg->need_reply = kmsg_desc.need_reply;

    if (kmsg_desc.message_size) {
        msg->message_buf = malloc(msg->message_buf_size);
        if (msg->message_buf == nullptr) {
            ret = -errno;
            return ret;
        }
    }

    fill_kbuf(&buf_desc, msg->message_id, msg, KBUF_FILL_MSG);
    // Acquire rwlock as a reader; WAITMSG does not need to acquire rdlock
    pthread_rwlock_rdlock(&mFdRwlock);
    if (ioctl(mEaselCommFd, EASELCOMM_IOC_READDATA, &buf_desc) == -1) {
        ALOGE("%s: READDATA failed (%d)", __FUNCTION__, errno);
        ret = -errno;
        free(msg->message_buf);
        msg->message_buf = nullptr;
        msg->message_buf_size = 0;
    }

    /*
     * If returning error and the message requests a DMA transfer, try to
     * discard the DMA transfer.
     */
    if (ret && kmsg_desc.dma_buf_size) {
        fill_kbuf(&buf_desc, kmsg_desc.message_id, nullptr, KBUF_FILL_UNUSED);
        if (ioctl(mEaselCommFd, EASELCOMM_IOC_RECVDMA, &buf_desc) == -1) {
            ALOGE("%s: RECVDMA failed (%d)", __FUNCTION__, errno);
        }
        msg->dma_buf_size = 0;
    }
    pthread_rwlock_unlock(&mFdRwlock);

    return ret;
}

// Send a reply to a message that expects one.
int EaselComm::sendReply(EaselMessage *origmessage, int replycode,
                         EaselMessage *replymessage) {
    struct easelcomm_kmsg_desc kmsg_desc;
    int ret = 0;

    kmsg_desc.message_id = 0;
    kmsg_desc.need_reply = false;
    kmsg_desc.in_reply_to = origmessage->message_id;
    kmsg_desc.replycode = replycode;

    if (replymessage) {
        kmsg_desc.message_size = replymessage->message_buf_size;
        kmsg_desc.dma_buf_size = replymessage->dma_buf_size;
        kmsg_desc.need_reply = replymessage->need_reply;
        assert(replymessage->need_reply == false);
    } else {
        kmsg_desc.message_size = 0;
        kmsg_desc.dma_buf_size = 0;
    }

    pthread_rwlock_rdlock(&mFdRwlock);
    ret = sendAMessage(mEaselCommFd, &kmsg_desc, replymessage);
    pthread_rwlock_unlock(&mFdRwlock);
    return ret;
}

// Receive a DMA transfer for an Easel Message that requests DMA.
// Returns 0 on success, -errno on EASELCOMM_IOC_RECVDMA failure
int EaselComm::receiveDMAImpl(const EaselMessage *msg, bool cancel) {
    struct easelcomm_kbuf_desc buf_desc;

#ifdef PROFILE_DMA
    struct timespec begin, end;
    long diff_us;
    clock_gettime(CLOCK_MONOTONIC, &begin);
#endif

    if (cancel) {
        ALOGD("%s: cancel receiving a DMA", __FUNCTION__);
        fill_kbuf(&buf_desc, msg->message_id, nullptr, KBUF_FILL_DMA);
    } else {
        fill_kbuf(&buf_desc, msg->message_id, msg, KBUF_FILL_DMA);
    }

    // Acquire rwlock as a reader
    pthread_rwlock_rdlock(&mFdRwlock);
    if (ioctl(mEaselCommFd, EASELCOMM_IOC_RECVDMA, &buf_desc) == -1) {
        pthread_rwlock_unlock(&mFdRwlock);
        ALOGE("%s: RECVDMA failed (%d)", __FUNCTION__, errno);
        return -errno;
    }
    pthread_rwlock_unlock(&mFdRwlock);

#ifdef PROFILE_DMA
    clock_gettime(CLOCK_MONOTONIC, &end);
    diff_us = (end.tv_sec - begin.tv_sec) * 1000000
            + (end.tv_nsec - begin.tv_nsec) / 1000;
    ALOGI("%s: receiveDMA done in %ld us, size=%zu",
          __FUNCTION__, diff_us, msg->dma_buf_size);
#endif

    return 0;
}

// Receive a DMA transfer for an Easel Message that requests DMA.
// Returns 0 on successful receiving, -errno on failure
int EaselComm::receiveDMA(const EaselMessage *msg) {
    return receiveDMAImpl(msg, /*cancel=*/false);
}

// Cancel receiving a DMA transfer for an Easel Message that requests DMA.
// Returns 0 on successful canceling, -errno on failure
int EaselComm::cancelReceiveDMA(const EaselMessage *msg) {
    return receiveDMAImpl(msg, /*cancel=*/true);
}

bool EaselComm::isConnected() {
    std::lock_guard<std::mutex> lock(mStatusMutex);
    return !mClosed;
}

/*
 * Open communications, register the Easel service ID.
 */
int EaselCommClient::open(EaselService service_id, long timeout_ms) {
    struct timespec begin;
    clock_gettime(CLOCK_MONOTONIC, &begin);

    std::lock_guard<std::mutex> lock(mStatusMutex);

    if (!mClosed) {
        return -EBUSY;
    }

    int retry = 0;
    while (1) {
        long diff_ms;
        struct timespec now;

        retry++;

        // Acquire rwlock as a writer
        pthread_rwlock_wrlock(&mFdRwlock);
        mEaselCommFd = ::open(kEaselCommDevPathClient, O_RDWR);
        pthread_rwlock_unlock(&mFdRwlock);

        clock_gettime(CLOCK_MONOTONIC, &now);
        diff_ms = (now.tv_sec - begin.tv_sec) * 1000
                  + (now.tv_nsec - begin.tv_nsec) / 1000000;

        if (mEaselCommFd > 0) {
            break;
        }
        if (diff_ms > timeout_ms) {
            ALOGE("%s: Failed to open device %s, retried %d",
                  __FUNCTION__, kEaselCommDevPathClient, retry);
            return -ETIME;
        }
        usleep(kOpenPollIntervalUs);   // Sleep to reduce retry attempts
    }

    // Acquire rwlock as a writer
    pthread_rwlock_wrlock(&mFdRwlock);
    if (ioctl(mEaselCommFd, EASELCOMM_IOC_REGISTER, service_id) < 0) {
        int ret = -errno;
        ::close(mEaselCommFd);
        pthread_rwlock_unlock(&mFdRwlock);
        ALOGE("%s: Failed to register service %d (%d)",
              __FUNCTION__, service_id, ret);
        return ret;
    }
    pthread_rwlock_unlock(&mFdRwlock);
    mClosed = false;

    return 0;
}

int EaselCommServer::open(EaselService service_id, long timeout_ms) {
    struct timespec begin;
    clock_gettime(CLOCK_MONOTONIC, &begin);

    std::lock_guard<std::mutex> lock(mStatusMutex);

    if (!mClosed) {
        return -EBUSY;
    }

    int retry = 0;
    while (1) {
        long diff_ms;
        struct timespec now;

        retry++;

        // Acquire rwlock as a writer
        pthread_rwlock_wrlock(&mFdRwlock);
        mEaselCommFd = ::open(kEaselCommDevPathServer, O_RDWR);
        pthread_rwlock_unlock(&mFdRwlock);

        clock_gettime(CLOCK_MONOTONIC, &now);
        diff_ms = (now.tv_sec - begin.tv_sec) * 1000
                  + (now.tv_nsec - begin.tv_nsec) / 1000000;

        if (mEaselCommFd > 0) {
            break;
        }
        if (diff_ms > timeout_ms) {
            ALOGE("%s: Failed to open device %s, retried %d",
                  __FUNCTION__, kEaselCommDevPathServer, retry);
            return -ETIME;
        }
        usleep(kOpenPollIntervalUs);   // Sleep to reduce retry attempts
    }

    // Acquire rwlock as a writer
    pthread_rwlock_wrlock(&mFdRwlock);
    if (ioctl(mEaselCommFd, EASELCOMM_IOC_REGISTER, service_id) < 0) {
        int ret = -errno;
        ::close(mEaselCommFd);
        pthread_rwlock_unlock(&mFdRwlock);
        ALOGE("%s: Failed to register service %d (%d)",
              __FUNCTION__, service_id, ret);
        return ret;
    }
    pthread_rwlock_unlock(&mFdRwlock);
    mClosed = false;

    return 0;
}

// Close connection.
void EaselComm::close() {
    {
        std::lock_guard<std::mutex> lock(mStatusMutex);
        if (mClosed) {
            return;
        }
        // Acquire rwlock as a writer
        pthread_rwlock_wrlock(&mFdRwlock);
        ioctl(mEaselCommFd, EASELCOMM_IOC_SHUTDOWN);
        ::close(mEaselCommFd);
        mEaselCommFd = -1;
        pthread_rwlock_unlock(&mFdRwlock);
        mClosed = true;
    }

    if (mHandlerThread.joinable()) {
        mHandlerThread.join();
    }
}

// Flush connection.
void EaselComm::flush() {
    // Acquire rwlock as a reader
    pthread_rwlock_rdlock(&mFdRwlock);
    ioctl(mEaselCommFd, EASELCOMM_IOC_FLUSH);
    pthread_rwlock_unlock(&mFdRwlock);
}

int EaselComm::startMessageHandlerThread(
        std::function<void(EaselMessage *msg)> callback) {
    if (mHandlerThread.joinable()) {
        return -EBUSY;
    }

    std::lock_guard<std::mutex> lock(mStatusMutex);
    if (mClosed) {
        return -EINVAL;
    }
    mHandlerThread = std::thread(&EaselComm::handleReceivedMessages, this, callback);
    return 0;
}

void EaselComm::joinMessageHandlerThread() {
    if (mHandlerThread.joinable()) {
        mHandlerThread.join();
    }
}

void EaselComm::handleReceivedMessages(
        std::function<void(EaselMessage *msg)> callback) {

    EaselMessage msg;
    while (isConnected()) {
        int ret = receiveMessage(&msg);
        if (ret != 0) {
          break;
        }
        if (msg.message_buf == nullptr) {
          continue;
        }
        callback(&msg);
        if (msg.message_buf != nullptr) {
            free(msg.message_buf);
            msg.message_buf = nullptr;
        }
    }
}

// Client side handshaking.
// Send, receive, send.
int EaselCommClient::initialHandshake() {
    EaselMessage msg;
    int ret = 0;
    const int32_t timeoutMsHandshakeClient = 1000;  // Client waits for 1 sec

    composeHandshake(&msg, 0);
    ret = sendMessage(&msg);
    if (ret) {
        return ret;
    }

    msg.timeout_ms = timeoutMsHandshakeClient;
    ret = receiveMessage(&msg);
    if (ret) {
        return ret;
    }
    ret = verifyHandshake(&msg, 1);
    if (ret) {
        return ret;
    }

    composeHandshake(&msg, 2);
    ret = sendMessage(&msg);
    return ret;
}

// Server side handshaking.
// Receive, send, receive
int EaselCommServer::initialHandshake() {
    EaselMessage msg;
    int ret = 0;
    const int32_t timeoutMsHandshakeServer = 5000;  // Server waits for 5 sec

    msg.timeout_ms = timeoutMsHandshakeServer;
    ret = receiveMessage(&msg);
    if (ret) {
        return ret;
    }
    ret = verifyHandshake(&msg, 0);
    if (ret) {
        return ret;
    }

    composeHandshake(&msg, 1);
    ret = sendMessage(&msg);
    if (ret) {
        return ret;
    }

    msg.timeout_ms = timeoutMsHandshakeServer;
    ret = receiveMessage(&msg);
    if (ret) {
        return ret;
    }
    return verifyHandshake(&msg, 2);
}
