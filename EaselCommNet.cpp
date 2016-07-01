/* TCP/IP network-based mock implementation of Android/Easel communication. */


#define _BSD_SOURCE
#include <endian.h>

#include "mockeaselcomm.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <list>
#include <map>
#include <mutex>
#include <thread>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/ip.h>

namespace {

// Client thread that reads and handles incoming control messages
void clientControlMessageHandlerThread(EaselCommClientNet *easelcomm) {
    easelcomm->controlMessageHandlerLoop();
}

// Spawn the client control message handling thread
void spawnClientMessageHandlerThread(EaselCommClientNet *easelcomm) {
    easelcomm->mMessageHandlerThread =
        new std::thread(clientControlMessageHandlerThread, easelcomm);
}

// Server thread that reads and handles incoming control messages
void serverControlMessageHandlerThread(EaselCommServerNet *easelcomm) {
    while (true) {
        int ret = easelcomm->controlMessageHandlerLoop();
        /*
         * If message handler exited for reason other than client disconnect,
         * bail.
         */
        if (ret != -ESHUTDOWN) {
            break;
        }
        // Client disconnected, wait for another client connection.
        ret = easelcomm->waitForClientConnect();
        if (ret < 0) {
            break;
        }
    }
}

// Spawn the server control message handling thread
void spawnServerMessageHandlerThread(EaselCommServerNet *easelcomm) {
    easelcomm->mMessageHandlerThread =
        new std::thread(serverControlMessageHandlerThread, easelcomm);
}

} // anonymous namespace

/*
 * ------------------------------------------------------------------------
 * EaselCommNet mock EaselComm implementation for TCP/IP.
 */

/*
 * Initial state for newly constructed object, or server resetting after
 * client disconnects, waiting for next client connect.
 */
void EaselCommNet::reinit() {
    mSequenceNumberIn = 0;
    mSequenceNumberOut = 0;
    mNextMessageId = 0;
    mShuttingDown = false;
}

EaselCommNet::EaselCommNet() : mServiceId(-1), mServicePort(PORT_DEFAULT) {
    reinit();
}

/*
 * Close communication socket.  Run some sanity checks, print warnings if
 * connection doesn't look quiescent, discard state specific to the old
 * connection.  The server can then accept another client connection after
 * this.
 */
void EaselCommNet::closeConnection() {
    ::shutdown(mConnectionSocket, SHUT_RDWR);
    ::close(mConnectionSocket);

    {
        std::lock_guard<std::mutex> lk(mMessageQueueLock);

        if (!mMessageQueue.empty())
            fprintf(stderr,
                    "easelcomm: service %d closing connection with non-empty"
                    " message queue, discarding...\n", mServiceId);

        for (std::list<IncomingDataXfer *>::iterator it =
                 mMessageQueue.begin(); it != mMessageQueue.end(); ++it) {
            uint64_t msgid = be64toh((*it)->message->message_id);
            fprintf(stderr, "message ID %" PRIu64 ": size %u DMA size %u\n",
                  msgid, be32toh((*it)->message->message_buf_size),
                  be32toh((*it)->message->dma_buf_size));
        }

        mMessageQueue.clear();
    }
    {
        std::lock_guard<std::mutex> lk(mDmaDataMapLock);
        for (std::map<EaselComm::EaselMessageId, char *>::iterator it =
                 mDmaDataMap.begin(); it != mDmaDataMap.end(); ++it)
            fprintf(stderr,
                    "easelcomm: service %d closing connection with unread DMA"
                    " transfer for message ID %" PRIu64 "\n", mServiceId,
                    it->first);
        mDmaDataMap.clear();
    }

    {
        std::lock_guard<std::mutex> lk(mSendWaitingMapLock);
        for (std::map<EaselComm::EaselMessageId,
                 OutgoingDataXfer *>::iterator it = mSendWaitingMap.begin();
             it != mSendWaitingMap.end(); ++it) {
            fprintf(stderr,
                    "easelcomm: service %d closing connection with data"
                    " transfer originator waiting for message ID %" PRIu64
                    " to complete\n", mServiceId, it->first);

            OutgoingDataXfer *out_xfer = it->second;
            std::lock_guard<std::mutex> xferlock(out_xfer->xfer_done_lock);
            /*
             * Wakeup waiter with transfer done indication.
             */
            out_xfer->xfer_done = true;
            out_xfer->xfer_done_cond.notify_one();
        }
    }
}

/*
 * Close communications, flag that service is shutting down and wakeup any
 * receiveMessage() waiters to let the callers exit gracefully.  Called for
 * the close() method of clients and servers.  A server is expected to call
 * open() prior to resuming service after this.
 */
void EaselCommNet::closeService() {
    /*
     * HACK: Delay prior to connection shutdown to let pending transfers
     * complete on remote side, prior to triggering remote's shutdown
     * shutdown handling.  TCP/IP conection shutdown has effects on the
     * remote side that won't be present in the "real Easel" comm lib.
     */
    std::this_thread::sleep_for(std::chrono::seconds(1));
    closeConnection();

    {
        /* Set shutdown flag, wakeup mMessageQueue waiters */
        std::lock_guard<std::mutex> lk(mMessageQueueLock);
        mShuttingDown = true;
        mMessageQueueArrivalCond.notify_all();
    }
}

/*
 * Write a control message, consisting of a control command and optionally
 * fixed-length arguments that accompany the command.  App-supplied
 * variable-length data that goes along with the command (like message
 * buffers), if any, is written separately via writeExtra below.
 *
 * Must be called with mConnectionOutLock held.
 */
int EaselCommNet::writeMessage(int command, const void *args, int args_len) {
    ControlMessage message;
    message.sequence_no = htobe64(mSequenceNumberOut++);
    message.service_id = htobe32(mServiceId);
    message.command = htobe32(command);
    message.command_arg_len = htobe32(args_len);

    int ret =
        TEMP_FAILURE_RETRY(send(mConnectionSocket, &message,
                                sizeof(message), 0));
    if (ret < 0) {
        perror("send");
        return ret;
    }
    assert(ret == sizeof(message));

    if (args_len) {
        ret = TEMP_FAILURE_RETRY(send(mConnectionSocket, args, args_len, 0));
        if (ret < 0) {
            perror("send");
            return ret;
        }
        assert(ret == args_len);
    }

    return 0;
}

/*
 * Write extra variable-length command data that follows the fixed-length
 * start of a control command argument, such as DMA data.
 *
 * Must be called with mConnectionOutLock held.
 */
int EaselCommNet::writeExtra(const void *extra_data, int extra_len) {
    if (!extra_data || !extra_len)
        return 0;

    int ret = TEMP_FAILURE_RETRY(
        send(mConnectionSocket, extra_data, extra_len, 0));
    if (ret < 0) {
        perror("send");
        return ret;
    }
    assert(ret == extra_len);
    return 0;
}

/*
 * Read the requested number of bytes from the remote.
 *
 * Only called by a single incoming message handler thread, not locked.
 */
int EaselCommNet::readBytes(char *dest, ssize_t len) {
    while (len) {
        ssize_t ret = TEMP_FAILURE_RETRY(recv(mConnectionSocket, dest, len, 0));
        if (ret < 0) {
            perror("recv");
            return ret;
        }
        if (!ret) {
            fprintf(stderr,
                    "easelcomm: service %d remote has shut down\n",
                    mServiceId);
            closeConnection();
            errno = ESHUTDOWN;
            return -1;
        }

        dest += ret;
        len -= ret;
    }

    return 0;
}

/*
 * Read the fixed-length part of a control message from the remote.
 *
 * Only called by a single incoming message handler thread, not locked.
 */
int EaselCommNet::readMessage(ControlMessage *message, void **args) {
    int ret;
    *args = nullptr;

    ret = readBytes((char *)message, sizeof(ControlMessage));
    if (ret)
        return ret;
    assert(be64toh(message->sequence_no) == mSequenceNumberIn);
    mSequenceNumberIn++;

    // We implement a single service per TCP connection.
    assert(be32toh(message->service_id) == mServiceId);

    int arg_len = be32toh(message->command_arg_len);

    if (arg_len) {
        *args = (char *)malloc(arg_len);
        if (*args == nullptr)
            return -1;
        ret = readBytes((char *)*args, arg_len);
        if (ret)
            return ret;
    }

    return 0;
}

/*
 * Wakeup request originator waiting on recipient to finish the requested DMA
 * transfer and/or reply to a message needing a reply.  Called when the
 * CMD_DMA_DONE arrives indicating the recipient has completed the DMA
 * transfer, and/or when a CMD_SEND_DATA_XFER arrives for a message that is a
 * reply.  If this is a DMA done event but a reply is still needed, the waiter
 * is not woken up.
 *
 * reply_xfer points to the reply data transfer if this is a "reply received"
 * wakeup, or nullptr if this is a "DMA Done" wakeup.
 */

void EaselCommNet::wakeupSender(EaselComm::EaselMessageId message_id,
                                IncomingDataXfer *reply_xfer) {
    // Find waiting request originator in mSendWaitingMap.
    std::unique_lock<std::mutex> maplock(mSendWaitingMapLock);
    OutgoingDataXfer *out_xfer = mSendWaitingMap[message_id];
    assert(out_xfer);
    /*
     * Lock originating transfer info, then drop lock on waiter map, to avoid
     * race between DMA done and reply received.
     */
     std::lock_guard<std::mutex> xferlock(out_xfer->xfer_done_lock);
     maplock.unlock();
    /*
     * If This is a DMA done event but a reply is still needed, don't wakeup
     * yet.
     */
    if (out_xfer->need_reply && !reply_xfer)
        return;
    /*
     * Wakeup waiter with transfer done indication, and set pointer to the
     * reply if this is a need_reply request waiting on a reply.
     */
    out_xfer->xfer_done = true;

    if (reply_xfer)
        out_xfer->reply_xfer = reply_xfer;

    out_xfer->xfer_done_cond.notify_one();
}

/* CMD_DMA_DONE received for DMA Done, wakeup waiting originator */
void EaselCommNet::handleDmaDone(DmaDoneArgs *dd) {
  EaselComm::EaselMessageId message_id = be64toh(dd->message_id);
    wakeupSender(message_id, nullptr);
}

/* Return the next outgoing message ID */
EaselComm::EaselMessageId EaselCommNet::getNextMessageId() {
    return mNextMessageId++;
}

/*
 * Send outgoing transfer, block on DMA completion or reply receipt if needed.
 */
int EaselCommNet::sendXferAndWait(
    const EaselMessage *msg, const EaselMessage *inreplyto,
    OutgoingDataXfer *out_xfer, IncomingDataXfer **reply_xfer,
    int reply_code) {
    int ret;

    // Setup the CMD_SEND_DATA_XFER control message args
    SendDataXferArgs send_args;
    send_args.message_id = htobe64(out_xfer->message_id);
    send_args.need_reply = reply_xfer ? true : false;
    send_args.is_reply = inreplyto ? true : false;
    send_args.replied_to_id = htobe64(inreplyto ? inreplyto->message_id : 0);
    send_args.replycode = htobe32(reply_code);
    send_args.message_buf_size = htobe32(msg ? msg->message_buf_size : 0);
    send_args.dma_buf_size =
        htobe32(msg ? (msg->dma_buf ? msg->dma_buf_size : 0) : 0);

    {
        /*
         * Send the CMD_SEND_DATA_XFER control message, followed by message
         * buffer.
         */
        std::lock_guard<std::mutex> lk(mConnectionOutLock);
        ret = writeMessage(CMD_SEND_DATA_XFER, &send_args,
                           sizeof(SendDataXferArgs));
        if (ret < 0)
            return -1;

        if (msg) {
            ret = writeExtra(msg->message_buf, msg->message_buf_size);
            if (ret < 0)
                return -1;

            // Net mockup just appends the DMA data to the request.
            if (msg && msg->dma_buf && msg->dma_buf_size) {
                ret = writeExtra(msg->dma_buf, msg->dma_buf_size);
                if (ret < 0)
                    return -1;
            }
        }
    }

    // Block on reply received/DMA done from remote if needed.
    if ((msg && msg->dma_buf && msg->dma_buf_size) || reply_xfer) {
        std::unique_lock<std::mutex> lk(out_xfer->xfer_done_lock);
        out_xfer->xfer_done_cond.wait(lk, [&]{return out_xfer->xfer_done;});
    }

    return 0;
}

/*
 * Send a data transfer request, either a no-reply message, a need_reply
 * message, or a reply message.  If the message has a DMA transfer and/or
 * needs a reply then blocking waiting for the DMA completion or reply.
 */
int EaselCommNet::sendXfer(
    const EaselMessage *msg, const EaselMessage *inreplyto,
    IncomingDataXfer **reply_xfer, int reply_code) {
    /*
     * Setup the outgoing transfer, place into waiter map, even if don't
     * need to wait (could optimize that out with another conditional).
     */
    OutgoingDataXfer out_xfer;
    out_xfer.xfer_done = false;
    out_xfer.reply_xfer = nullptr;
    out_xfer.message_id = getNextMessageId();
    out_xfer.need_reply = reply_xfer ? true : false;

    {
        std::lock_guard<std::mutex> lk(mSendWaitingMapLock);
        mSendWaitingMap[out_xfer.message_id] = &out_xfer;
    }

    int sendret = sendXferAndWait(msg, inreplyto, &out_xfer, reply_xfer,
                                  reply_code);

    // Remove sender data transfer info from waiter map.
    {
        std::lock_guard<std::mutex> lk(mSendWaitingMapLock);
        mSendWaitingMap.erase(out_xfer.message_id);
    }

    if (reply_xfer) {
        if (out_xfer.reply_xfer)
            *reply_xfer = out_xfer.reply_xfer;
        else
            sendret = -1;
    }

    return sendret;
}

/*
 * Handle incoming CMD_SEND_DATA_XFER, a new data transfer request has
 * arrived.  Setup the incoming data transfer and send to appropriate
 * receiver, either the originator of the message to which this is a reply or
 * to the receiveMessage() queue.
 */
int EaselCommNet::handleIncomingDataXfer(SendDataXferArgs *send_args) {
    int ret;
    char *dmadata = nullptr;
    EaselMessage *message;

    IncomingDataXfer *in_xfer =
        (IncomingDataXfer *)malloc(sizeof(IncomingDataXfer));
    if (in_xfer == nullptr)
        return -1;

    message = (EaselMessage *)malloc(sizeof(EaselMessage));
    if (message == nullptr) {
        free(in_xfer);
        return -1;
    }

    message->message_buf = nullptr;
    message->message_buf_size = be32toh(send_args->message_buf_size);
    message->dma_buf = nullptr;
    message->dma_buf_size = be32toh(send_args->dma_buf_size);
    message->message_id = be64toh(send_args->message_id);
    message->need_reply = send_args->need_reply;

    // Read the message buffer, if any.
    if (message->message_buf_size) {
        message->message_buf = malloc(message->message_buf_size);
        if (message->message_buf == nullptr) {
            free(message);
            free(in_xfer);
            return -1;
        }
        ret = readBytes((char *)message->message_buf,
                        message->message_buf_size);
        if (ret < 0) {
            free(message->message_buf);
            free(message);
            free(in_xfer);
            return -1;
        }
    }

    // Read the DMA data, if any, and stash in mDmaDataMap for retrieval later.
    if (message->dma_buf_size) {
        dmadata = (char *)malloc(message->dma_buf_size);
        if (dmadata == nullptr) {
            free(message->message_buf);
            free(message);
            free(in_xfer);
            return -1;
        }
        ret = readBytes(dmadata, message->dma_buf_size);
        if (ret < 0) {
            free(dmadata);
            free(message->message_buf);
            free(message);
            free(in_xfer);
            return -1;
        }
        {
            std::lock_guard<std::mutex> lk(mDmaDataMapLock);
            mDmaDataMap[message->message_id] = dmadata;
        }
    }

    in_xfer->send_args = send_args;
    in_xfer->message = message;

    /*
     * If this is a reply to a previous message, send it to the originator
     * waiting on the reply.
     */
    if (send_args->is_reply) {
        wakeupSender(be64toh(send_args->replied_to_id), in_xfer);
        /*
         * in_xfer, send_args, and message are freed after initiator grabs the
         * reply info.
         */
    } else {
        // Put in the general incoming message queue and signal its arrival.
        std::lock_guard<std::mutex> lk(mMessageQueueLock);
        mMessageQueue.push_back(in_xfer);
        mMessageQueueArrivalCond.notify_one();
        /*
         * in_xfer, send_args, and message are freed when receiveMessage is done
         * copying the request info to its caller.
         */
    }

    return 0;
}

// Handle an incoming control command.
void EaselCommNet::handleCommand(int command, void *args) {
    switch (command) {
    case CMD_SEND_DATA_XFER:
        // Incoming data transfer.
        if (handleIncomingDataXfer((SendDataXferArgs *)args) < 0) {
            free(args);
        }
        // else args will be freed when data transfer handling is done.
        break;
    case CMD_DMA_DONE:
        // DMA done for a request we originated.
        handleDmaDone((DmaDoneArgs *)args);
        free(args);
        break;
    default:
        fprintf(stderr, "easelcomm: invalid command code %d received\n",
                command);
        assert(0);
    }
}

// Read and handle incoming control message
int EaselCommNet::controlMessageHandlerLoop() {
    int ret;

    while (true) {
        ControlMessage message;
        void *args = nullptr;
        ret = readMessage(&message, &args);
        if (ret < 0) {
            ret = -errno;
            break;
        }
        handleCommand(be32toh(message.command), args);
    }

    return ret;
}

// Send a message without waiting for a reply.
int EaselCommNet::sendMessage(const EaselMessage *msg) {
    return sendXfer(msg, nullptr, nullptr, 0);
}

// Send a message and wait for a reply.
int EaselCommNet::sendMessageReceiveReply(
    const EaselMessage *msg, int *replycode, EaselMessage *reply) {
    IncomingDataXfer *reply_xfer = nullptr;

    int ret = sendXfer(msg, nullptr, &reply_xfer, 0);
    if (ret < 0)
        return ret;
    assert(reply_xfer);

    if (replycode)
      *replycode = be32toh(reply_xfer->send_args->replycode);

    if (reply)
        *reply = *reply_xfer->message;

    free(reply_xfer->message);
    free(reply_xfer->send_args);
    free(reply_xfer);
    return 0;
}

// Wait for and return next incoming Easel Message.
int EaselCommNet::receiveMessage(EaselMessage *msg) {
    std::unique_lock<std::mutex> lk(mMessageQueueLock);

    // Wait for message to arrive or shutdown
    mMessageQueueArrivalCond.wait(
        lk, [this]{return !mMessageQueue.empty() || mShuttingDown;});

    if (mShuttingDown) {
        errno = ESHUTDOWN;
        // Just to clear out any junk in caller's param
        msg->message_buf = 0;
        msg->message_buf_size = 0;
        msg->dma_buf_size = 0;
        return -1;
    }

    IncomingDataXfer *in_xfer = mMessageQueue.front();
    assert(in_xfer);
    mMessageQueue.pop_front();
    lk.unlock();

    // Copy request to caller
    *msg = *in_xfer->message;
    // Sanity enforcement
    msg->dma_buf = nullptr;

    // Free the incoming data transfer info
    free(in_xfer->message);
    free(in_xfer->send_args);
    free(in_xfer);
    return 0;
}

// Send a reply to a message that expects one.
int EaselCommNet::sendReply(EaselMessage *origmessage, int replycode,
                            EaselMessage *replymessage) {
    return sendXfer(replymessage, origmessage, nullptr, replycode);
}

// Receive a DMA transfer for an Easel Message that requests DMA.
int EaselCommNet::receiveDMA(const EaselMessage *msg) {
    char *src;
    {
        // Find the data in mDmaDataMap by message ID and erase the map entry.
        std::lock_guard<std::mutex> lk(mDmaDataMapLock);
        src = mDmaDataMap[msg->message_id];
        assert(src);
        mDmaDataMap.erase(msg->message_id);
    }
    if (msg->dma_buf) {
        // Copy the DMA data to the caller-supplied buffer.
        memcpy(msg->dma_buf, src, msg->dma_buf_size);
    }

    // Send DMA_DONE to initiator.
    DmaDoneArgs dd;
    dd.message_id = htobe64(msg->message_id);
    std::lock_guard<std::mutex> lk(mConnectionOutLock);
    int netret = writeMessage(CMD_DMA_DONE, &dd, sizeof(dd));
    return netret;
}

/*
 * ------------------------------------------------------------------------
 * EaselCommClientNet
 */

/*
 * Open communications, register the Easel service ID.  Still need to
 * connect().
 */
int EaselCommClientNet::open(int service_id) {
    mServiceId = service_id;
    mShuttingDown = false;
    return 0;
}

// Close connection.
void EaselCommClientNet::close() {
    closeService();
    /*
     * HACK: After client closes a connection, delay to allow server to
     * process the connection shutdown and start listening for a new
     * connection.  This is intended primarily to support back-to-back
     * disconnect and reconnect sequences from automated tests.  Since the
     * TCP/IP mock doesn't continuously listen for new connections and
     * process simultaneous clients connected to the same port, as would a
     * more normal network service, we serialize these operations using this
     * hack.
     */
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

// Network connector to "Easel" server by hostname.
int EaselCommClientNet::connect(const char *serverhost) {
    return connect(serverhost, PORT_DEFAULT);
}

// Network connector to "Easel" server by hostname and TCP port.
int EaselCommClientNet::connect(const char *serverhost, int port) {
    if (serverhost == nullptr)
        serverhost = "localhost";
    printf("easelcomm: service %d client connecting to %s:%d...\n", mServiceId,
           serverhost, port);

    mConnectionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mConnectionSocket < 0)
        return -1;

    struct hostent *hostent = gethostbyname(serverhost);
    if (!hostent) {
        closeConnection();
        return -1;
    }

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    // Use first address returned, is IPV4 address.
    memcpy(&sa.sin_addr.s_addr, *hostent->h_addr_list, hostent->h_length);

    int ret = ::connect(mConnectionSocket, (const struct sockaddr *)&sa,
                        sizeof(sa));
    if (ret < 0) {
        closeConnection();
        return -1;
    }

    printf("easelcomm: service %d client connected\n", mServiceId);
    spawnClientMessageHandlerThread(this);
    return 0;
}

/*
 * ------------------------------------------------------------------------
 * EaselCommServer
 */

// Set TCP port to listen on.
void EaselCommServerNet::setListenPort(int port) {
    mServicePort = port;
}

/*
 * Reset server state and wait for new client connection.  Returns when a
 * new connection is established.
 */
int EaselCommServerNet::waitForClientConnect() {
    reinit();

    printf("easelcomm: service %d server accepting connections on port %d\n",
           mServiceId, mServicePort);
    mConnectionSocket = accept(mListenSocket, nullptr, nullptr);
    if (mConnectionSocket < 0) {
        perror("accept");
        return -1;
    }
    printf("easelcomm: service %d connection established\n", mServiceId);
    return 0;
}

/*
 * Initialize communication, register Easel service ID, wait for client
 * connection.
 */
int EaselCommServerNet::open(int service_id) {
    mServiceId = service_id;
    mShuttingDown = false;

    mListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mListenSocket < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(mServicePort);
    sa.sin_addr.s_addr = INADDR_ANY;
    int ret = bind(mListenSocket, (struct sockaddr *)&sa, sizeof(sa));
    if (ret < 0) {
        perror("bind");
        ::close(mListenSocket);
        return -1;
    }

    ret = listen(mListenSocket, 1);
    if (ret < 0) {
        perror("listen");
        ::close(mListenSocket);
        return -1;
    }

    ret = waitForClientConnect();
    if (ret)
        return ret;

    spawnServerMessageHandlerThread(this);
    return 0;
}

void EaselCommServerNet::close() {
    closeService();
    ::close(mListenSocket);
}
