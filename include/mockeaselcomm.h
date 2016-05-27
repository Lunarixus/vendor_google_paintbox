#ifndef GOOGLE_PAINTBOX_MOCKEASELCOMM_H
#define GOOGLE_PAINTBOX_MOCKEASELCOMM_H

/* TCP/IP-based mock AP/Easel transport */

#include "easelcomm.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <list>
#include <map>
#include <mutex>
#include <thread>
#include <sys/types.h>

// TCP/IP mock implementation of EaselComm
class EaselCommNet : public EaselComm {
public:
    /*
     * Send a message to remote.  Returns once the message is sent and the
     * remote has received the DMA transfer, if any.
     *
     * msg->message_buf points to the message buffer to be sent.
     * msg->message_buf_size is the size of the message buffer in bytes.
     * msg->dbuf_size is the size of the DMA transfer in bytes, or zero if none.
     * Other *msg fields are not used by this function.
     *
     * Returns 0 for success, -1 for failure.  Failure generally means Easel
     * is unresponsive or in an invalid state, and Easel is being reset.
     */
    int sendMessage(const EaselMessage *msg) override;

    /*
     * Send a message to remote and wait for a reply.
     *
     * msg->message_buf points to the message buffer to be sent.
     * msg->message_buf_size is the size of the message buffer in bytes.
     * msg->dbuf_size is the size of the DMA transfer in bytes, or zero if none.
     * Other *msg fields are not used by this function.
     *
     * replycode returns the application-specific replycode from the
     * reply sent back by the receiver.  This can be nullptr if not used.
     *
     * reply optionally receives the EaselMessage sent in reply.  If used, it
     * may have have a message buffer (if reply->message_buf is not nullptr)
     * and/or may request a DMA transfer (if reply->dbuf_size is non-zero).
     * If reply->message_buf is not nullptr, callers frees it when done
     * processing the message buffer.  If the receiver does not return a reply
     * message then this parameter can be nullptr.
     *
     * Returns 0 for success, -1 for failure.  Failure generally means Easel
     * is unresponsive or in an invalid state, and Easel is being reset.
     */
    int sendMessageReceiveReply(const EaselMessage *msg, int *replycode,
                                EaselMessage *reply) override;

    /*
     * Wait for the next message from remote to arrive.
     *
     * msg->message_buf points to the message received.
     * msg->message_buf_size is the size of the message in bytes.
     * msg->dbuf_size is the size of the DMA transfer in bytes, or zero if none.
     *   If non-zero, receiveDMA must be called to complete the transfer and
     *   free the state associated with the outstanding DMA transfer.
     * msg->needreply is true if sender is waiting for a reply.
     * Other *msg fields are not used or used internally by this function.
     *
     * The caller frees msg->message_buf when done processing the message
     * buffer.
     *
     * Returns 0 for success, -1 for failure.  On failure,
     * if ERRNO == ESHUTDOWN then the connection is being shut down, as with
     * deinit().  Other failure generally means Easel is unresponsive or in an
     * invalid state, and Easel is being reset.
     */
    int receiveMessage(EaselMessage *msg) override;

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
    * Returns 0 for success, -1 for failure.  Failure generally means Easel
    * is unresponsive or in an invalid state, and Easel is being reset.
    */
    int sendReply(EaselMessage *msg, int replycode, EaselMessage *replymsg)
         override;

   /*
    * Read the DMA transfer requested by the remote.  Called when
    * receiveMessage() returns a message with dmasz != 0.
    * Returns when the DMA transfer is complete.
    *
    * msg points to the message returned by receiveMessage(), with field
    * msg->dbuf set to the DMA destination buffer address.  That buffer must
    * have at least msg->dbusz bytes.  Or, if msg->dbuf is NULL, discard the
    * DMA transaction without receiving.
    *
    * Returns 0 for success, -1 for failure.  Failure generally means Easel
    * is unresponsive or in an invalid state, and Easel is being reset.
    */
    int receiveDMA(const EaselMessage *msg) override;

    // Control message handler thread.
    std::thread *mMessageHandlerThread;

    /* Handle incoming control messages */
    void controlMessageHandlerLoop();

protected:
    EaselCommNet();
    ~EaselCommNet() {};

    // Default port for Easel remote service mockups via TCP/IP
    const static int PORT_DEFAULT = 4242;

    // Control message commands
    enum {
        CMD_SEND_DATA_XFER,  // data transfer send
        CMD_DMA_DONE,        // data transfer DMA done
    };

    // Control message invariant part
    struct ControlMessage {
        uint64_t sequence_no;     // sequence number of this control message
        uint32_t service_id;      // destination Easel service ID
        uint32_t command;         // control command code
        uint32_t command_arg_len; // # of bytes of command arguments that follow
    };

    // Data transfer request args, sent with CMD_SEND_DATA_XFER
    struct SendDataXferArgs {
        // initiating service's ID for the Easel message
        EaselComm::EaselMessageId message_id;
        // message buffer size in bytes
        uint32_t message_buf_size;
        // DMA buffer size in bytes (or zero if none requested)
        uint32_t dma_buf_size;
        // initiator is waiting for a reply to this message?
        bool need_reply;
        // this message is a reply to another need_reply=true message
        bool is_reply;
        // Easel message ID of the message being replied to (for a reply)
        EaselComm::EaselMessageId replied_to_id;
        // sendReply reply code to return to initiator (for a reply)
        uint32_t replycode;
        // followed by message buffer (buf_size bytes)
        // followed by DMA buffer (dma_buf_size bytes)
    };

    // CMD_DMA_DONE arguments
    struct DmaDoneArgs {
        EaselComm::EaselMessageId message_id; // which message is completed
    };

    /*
     * Bookkeeping for a data transfer for which the local system is the
     * receiver, containing the message plus some extra info via the
     * CMD_SEND_DATA_XFER arguments.
     *
     * If this is a reply then this struct is linked to the replied-to message
     * via the OutoingDataXfer field reply_xfer.  Once the initiator is woken
     * up and grabs the reply info, this struct is destroyed.
     *
     * A non-reply message is queued on inmessageq until a receiveMessage call
     * picks it up.
     */
    struct IncomingDataXfer {
        SendDataXferArgs *send_args;
        EaselComm::EaselMessage *message;
    };

    /*
     * A sender-side data transfer for which originator is waiting for a reply
     * and/or DMA transfer done indication from the recipient.  These structs
     * are placed into the mSendWaitingMap while the sender waits for that
     * indication.
     *
     * (If need_reply == false and dma_buf_size == 0 then the originator does
     * not wait for the recipient to do anything, and this struct is not used
     * for that data transfer.)
     */
    struct OutgoingDataXfer {
        // ID of this Easel Message
        EaselComm::EaselMessageId message_id;
        // is a reply needed?
        bool need_reply;
        // is tranfer done (both DMA done and reply received)?
        bool xfer_done;
        // pointer to reply received, nullptr if no reply yet or none needed
        IncomingDataXfer *reply_xfer;
        // signals transfer done to waiter
        std::condition_variable xfer_done_cond;
        // lock protects access to xfer_done and xfer_done_cond
        std::mutex xfer_done_lock;
    };

    // The Easel service to which this connection is bound.
    uint32_t mServiceId;
    // For servers: the TCP port to bind to.
    int mServicePort;
    // Socket for the connection with the remote side.
    int mConnectionSocket;
    /*
     * Mutex protecting control message write sequences to the connection
     * socket, which may consist of multiple send() calls.
     *
     * (Reads are performed by a single thread only, and are not locked).
     */
    std::mutex mConnectionOutLock;
    // Control message sequence numbers, for similarity to other transports.
    uint64_t mSequenceNumberIn;
    uint64_t mSequenceNumberOut;
    // Next outgoing message id
    std::atomic_ullong mNextMessageId;
    /*
     * Map of "DMA data" waiting for the recipient to call us to receive it.
     * Maps from an EaselMessageId to the sender's DMA data bytes.  When the
     * data transfer is received the "DMA buffer" comes along with it in the
     * TCP/IP transport.  We stash it here and wait for the receiveDMA() call.
     */
    std::map<EaselComm::EaselMessageId, char *>mDmaDataMap;
    std::mutex mDmaDataMapLock;
    /*
     * Map of senders waiting for the remote to process a data transfer
     * (either a reply to a need_reply message or a DMA done notification for a
     * data transfer with a DMA component).
     */
    std::map<EaselComm::EaselMessageId, OutgoingDataXfer *>mSendWaitingMap;
    std::mutex mSendWaitingMapLock;
    /*
     * Queue of incoming messages waiting to be retrieved by a receiveMessage()
     * call, and the lock and the condvar for blocking on message arrival.
     */
    std::list<IncomingDataXfer *>mMessageQueue;
    std::mutex mMessageQueueLock;
    std::condition_variable mMessageQueueArrivalCond;
    /*
     * Is connection being shutdown?  Used to evict mMessageQueue waiters.
     * Access is protected via mMessageQueueLock.
     */
    bool mShuttingDown;

    void closeConnection();
    int writeMessage(int command, const void *args, int args_len);
    int writeExtra(const void *extra_data, int extra_len);
    int readBytes(char *dest, ssize_t len);
    int readMessage(ControlMessage *message, void **args);
    void wakeupSender(EaselMessageId message_id,
                      IncomingDataXfer *reply_xfer);
    void handleDmaDone(DmaDoneArgs *dd);
    EaselMessageId getNextMessageId();
    int sendXferAndWait(const EaselMessage *msg, const EaselMessage *inreplyto,
                        OutgoingDataXfer *sendxferinfo,
                        IncomingDataXfer **replyxferinfo, int replycode);
    int sendXfer(const EaselMessage *msg, const EaselMessage *inreplyto,
                 IncomingDataXfer **replyxferinfo, int replycode);
    int handleIncomingDataXfer(SendDataXferArgs *send_args);
    void handleCommand(int command, void *args);
};

// Client for network-based mock Easel communication.
class EaselCommClientNet : public EaselCommNet {
public:
   /*
    * Initialize for the specified service.
    *
    * id is the Easel service ID for which the caller is the client.
    *
    * Returns 0 for success, -1 for failure.  Failure generally means Easel
    * is unresponsive or in an invalid state, and Easel is being reset.
    */
    int open(int service_id) override;

    /*
     * Close down communication via this object.  Cancel any pending
     * receiveMessage() on our registered service ID.  Can re-use this
     * same object to call init() again.
     */
    void close() override;
    /*
     * Connect to the TCP/IP mock Easel communication server on the specified
     * host, or localhost if nullptr.  The signature that supplies a TCP port
     * connects to the specified port; the other version uses PORT_DEFAULT.
     *
     * The connect() call can be made before or after open().  The close()
     * call will close this network connection.
     *
     * Returns 0 for success, -1 for failure (errno is set).
     */
    int connect(const char *serverhost);
    int connect(const char *serverhost, int port);
};

// Server extensions for network-based mock Easel communication.
class EaselCommServerNet : public EaselCommNet  {
public:
   /*
    * Initialize for the specified service.
    *
    * id is the Easel service ID for which the caller is the server.
    *
    * Returns 0 for success, -1 for failure.  Failure generally means Easel
    * is unresponsive or in an invalid state, and Easel is being reset.
    */
    int open(int service_id) override;

    /*
     * Close down communication via this object.  Cancel any pending
     * receiveMessage() on our registered service ID.  Can re-use this
     * same object to call init() again.
     */
    void close() override;

    /*
     * Specify TCP port for server side listen.  If not called then
     * PORT_DEFAULT is used.
     */
    void setListenPort(int port);

    /*
     * The network-based mock of EaselCommServer::init() has an additional
     * semantic in that it blocks waiting for a connection from a client
     * before returning.  If multiple services are run in the same process
     * then it may be necessary to init the different objects in different
     * threads.
     */
};
#endif // GOOGLE_PAINTBOX_MOCKEASELCOMM_H
