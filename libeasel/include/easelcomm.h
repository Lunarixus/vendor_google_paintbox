/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef GOOGLE_PAINTBOX_EASELCOMM_H
#define GOOGLE_PAINTBOX_EASELCOMM_H

/*
 * Communication API between Android and the Easel coprocessor hosting the
 * Paintbox IPU.
 */

#include <cstddef> // size_t
#include <cstdint> // uint64_t

#include <uapi/linux/google-easel-comm.h>

#define DEFAULT_OPEN_TIMEOUT_MS 5000

/* Defines and data types used by API clients and servers. */
class EaselComm {
public:
    /*
     * Easel service identifiers registered by clients and servers to
     * route messages to each other.  These values are taken from the
     * corresponding identifiers in the kernel header.
     */
    enum {
        // Easel system control
        EASEL_SERVICE_SYSCTRL = EASELCOMM_SERVICE_SYSCTRL,
        // Easel shell
        EASEL_SERVICE_SHELL = EASELCOMM_SERVICE_SHELL,
        // Used by unit tests
        EASEL_SERVICE_TEST = EASELCOMM_SERVICE_TEST,
        // HDR+ via Paintbox camera framework service
        EASEL_SERVICE_HDRPLUS = EASELCOMM_SERVICE_HDRPLUS,
    };

    /*
     * Easel message identifier, unique on the originating side of
     * the link.
     */
    typedef uint64_t EaselMessageId;

    /* An Easel message */
    struct EaselMessage {
        void  *message_buf;        // pointer to the message buffer
        size_t message_buf_size;   // size in bytes of the message buffer
        void *dma_buf;             // Type A: pointer to local DMA buffer source or dest
        int dma_buf_fd;            // Type B: fd for dma_buf handle
        int dma_buf_type;          // specify Type A or B
        size_t dma_buf_size;       // size of the DMA buffer transfer
        EaselMessageId message_id; // message ID
        bool need_reply;           // true if originator is waiting on a reply
        EaselMessage(): message_buf_size(0),
                        dma_buf(nullptr), dma_buf_fd(-1),
                        dma_buf_type(EASELCOMM_DMA_BUFFER_USER),
                        dma_buf_size(0),
                        message_id(0), need_reply(false) {};
    };

    /*
     * Send a message to remote.  Returns once the message is sent and the
     * remote has received the DMA transfer, if any.
     *
     * msg->message_buf points to the message buffer to be sent.
     * msg->message_buf_size is the size of the message buffer in bytes.
     * msg->dma_buf_size is the size of the DMA transfer in bytes, or zero if
     *    none.
     * Other *msg fields are not used by this function.
     *
     * Returns 0 for success, -1 for failure.
     */
    virtual int sendMessage(const EaselMessage *msg);

    /*
     * Send a message to remote and wait for a reply.
     *
     * msg->message_buf points to the message buffer to be sent.
     * msg->message_buf_size is the size of the message buffer in bytes.
     * msg->dma_buf_size is the size of the DMA transfer in bytes, or zero if
     *    none.
     * Other *msg fields are not used by this function.
     *
     * replycode returns the application-specific replycode from the
     * reply sent back by the receiver.  This can be nullptr if not used.
     *
     * reply optionally receives the EaselMessage sent in reply.  If used, it
     * may have have a message buffer (if reply->message_buf is not nullptr)
     * and/or may request a DMA transfer (if reply->dma_buf_size is non-zero).
     * If reply->message_buf is not nullptr, callers frees it when done
     * processing the message buffer.  If the receiver does not return a reply
     * message then this parameter can be nullptr.
     *
     * Returns 0 for success, -1 for failure.
     */
    virtual int sendMessageReceiveReply(
        const EaselMessage *msg, int *replycode, EaselMessage *reply);

    /*
     * Wait for the next message from remote to arrive.
     *
     * msg->message_buf points to the message received.
     * msg->message_buf_size is the size of the message in bytes.
     * msg->dma_buf_size is the size of the DMA transfer in bytes, or zero if
     *   none.  If non-zero, receiveDMA must be called to receive or discard
     *   the transfer, thereby freeing the state associated with the
     *   outstanding DMA transfer.
     * msg->needreply is true if sender is waiting for a reply.
     * Other *msg fields are not used or used internally by this function.
     * The caller frees msg->message_buf when done processing the message
     * buffer.
     *
     * Returns 0 for success, -1 for failure.  On failure:
     *    errno == ESHUTDOWN means the connection is being shut down
     */
    virtual int receiveMessage(EaselMessage *msg);

    /*
     * Send a reply to a message for which the remote is waiting on a reply.
     * msg is the originally received message being replied to.
     *
     * replycode is an integer reply code to be sent to the remote.
     *
     * replymsg is an optional message to send in reply.  The fields are used
     * in the same way as for sendMessage.  If nullptr is provided then the
     * remote receives an EaselMessage with an empty message buffer and DMA
     * buffer.
     *
     * Returns 0 for success, -1 for failure.
     */
    virtual int sendReply(EaselMessage *msg, int replycode,
                          EaselMessage *replymsg);

    /*
     * Read the DMA transfer requested by the remote.  Called when
     * receiveMessage() returns a message with dmasz != 0.
     * Returns when the DMA transfer is complete.
     *
     * msg points to the message returned by receiveMessage(), with field
     * msg->dma_buf set to the DMA destination buffer address.  That buffer must
     * have at least msg->dbusz bytes.  Or, if msg->dma_buf is NULL, discard the
     * DMA transaction without receiving.
     *
     * Returns 0 for success, -1 for failure.
     */
    virtual int receiveDMA(const EaselMessage *msg);

    /*
     * Open communications for the specified service.
     *
     * id is the Easel service ID for which the caller is the client.
     * timeout_ms is the waiting timeout when this function is blocking.
     * Whether or not to use this parameter is implementation specific.
     *
     * Returns 0 for success, -1 for failure.
     */
    virtual int open(int service_id, long timeout_ms) = 0;

    /*
     * Close down communication via this object.  Cancel any pending
     * receiveMessage() on our registered service ID.  Can re-use this
     * same object to call init() again.
     */
    virtual void close();

    /*
     * Discard any existing messages for the registered Easel service ID,
     * on both the local and remote sides of the link.  Returns when
     * the remote side acknowledges flushing its messages.
     */
    virtual void flush();

protected:
    virtual ~EaselComm() {};

    // File descriptor for easelcomm device
    int mEaselCommFd;
};

class EaselCommClient : public EaselComm {
public:
    virtual int open(int service_id,
                     long timeout_ms = DEFAULT_OPEN_TIMEOUT_MS) override;
};

class EaselCommServer : public EaselComm {
public:
    virtual int open(int service_id, __unused long timeout_ms
                                     = DEFAULT_OPEN_TIMEOUT_MS) override;
};

#endif // GOOGLE_PAINTBOX_EASELCOMM_H
