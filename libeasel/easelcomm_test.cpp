/*
 * EaselComm unit tests.
 *
 * If compiled with AP_CLIENT defined then client-side testing code is
 * compiled;  if compiled with EASEL_SERVER defined then server-side testing
 * code is compiled.
 *
 * If compiled with FAKE_EASEL_TEST defined, then both client- and server-side
 * test code is compiled.  A thread is spawned to act as the server; the
 * client connects to the service run on that thread.  The kernel driver must
 * be compiled in "fake Easel" mode, with both AP/client and Easel/server
 * interfaces using local memory copies between the two.  This is temporary
 * for development time.
 *
 * If compiled with MOCKEASEL defined then TCP/IP-based mock EaselCommNet
 * interfaces are used.  A thread is spawned to act as the server as above.
 *
 * All tests are expected to run identically on "real Easel", so long as
 * compatible client and server test processes are run on the AP and Easel.
 */

//#define DEBUG

#include "easelcomm.h"

#ifdef MOCKEASEL
#include "mockeaselcomm.h"
#endif

#include "gtest/gtest.h"

#include <condition_variable>
#include <thread>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>

#if defined(FAKE_EASEL_TEST) || defined(MOCKEASEL)
#define AP_CLIENT
#define EASEL_SERVER
#endif

namespace {
// Number of times to repeat the message passing/DMA test sequence
const int kMsgTestRepeatTimes = 1000;

#ifdef AP_CLIENT
#ifdef MOCKEASEL
EaselCommClientNet easelcomm_client;
#else
EaselCommClient easelcomm_client;
#endif
#endif

#ifdef EASEL_SERVER
#ifdef MOCKEASEL
EaselCommServerNet easelcomm_server;
#else
EaselCommServer easelcomm_server;
#endif
#endif

// Reply messages created using this template data structure
struct testreply {
    char *msgbuf;
    size_t msglen;
    char *dmabuf;
    size_t dmalen;
    int replycode;
};

// Test message transfers created using this template data structure
struct testxfer {
    char *msgbuf;
    size_t msglen;
    char *dmabuf;
    size_t dmalen;
    testreply replymsg;
};

#define NXFERS 6
// Test message transfers
testxfer testxfers[NXFERS] = {
    { "test transfer #1 message", 25, "and a DMA buffer", 17,
    { nullptr, 0, nullptr, 0, 0} }, // no reply

    { "#2 has a message but no DMA buffer", 35, nullptr, 0,
    { nullptr, 0, nullptr, 0, 0} }, // no reply

    { "message for #3", 15, "DMA for #3", 11,
    { "reply to message #3", 20, "reply-to-#3 DMA", 16, 1040 } },

    { "#4 needs a reply and has no DMA", 31, nullptr, 0,
    { "yes it is reply to message #4", 30, nullptr, 0, 1099} },

    { "#5: DISCARD DMA", 16, "this DMA to be discarded", 25,
    { nullptr, 0, nullptr, 0, 0} }, // no reply

    { "#6 needs reply, no DMA", 23, nullptr, 0,
    { "the reply must DISCARD DMA", 27, "discard me", 11, 540} },

};

// Which testxfers[] entry the sender is currently working on
int sender_xferindex = 0;
// Sender completed all iterations, signalled via condvar, protected by lock
bool sender_done;
std::condition_variable sender_done_cond;
std::mutex sender_done_lock;

// Which testxfers[] entry the receiver is currently working on
int receiver_xferindex = 0;
// How many messages the receiver processed across all iterations
int receiver_msgcount = 0;
// Receiver completed all iterations, as above for sender
bool receiver_done;
std::condition_variable receiver_done_cond;
std::mutex receiver_done_lock;

static void msg_test_sender_iteration(EaselComm *sender) {
    int ret;

    for (sender_xferindex = 0; sender_xferindex < NXFERS; sender_xferindex++) {
        EaselComm::EaselMessage msg;
        msg.message_buf = testxfers[sender_xferindex].msgbuf;
        msg.message_buf_size = testxfers[sender_xferindex].msglen;
        msg.dma_buf = testxfers[sender_xferindex].dmabuf;
        msg.dma_buf_size = testxfers[sender_xferindex].dmalen;
        msg.need_reply = testxfers[sender_xferindex].replymsg.msgbuf;

        if (msg.need_reply) {
            int replycode;
            EaselComm::EaselMessage reply;

            ret = sender->sendMessageReceiveReply(&msg, &replycode, &reply);
            ASSERT_TRUE(ret == 0);

#ifdef DEBUG
            printf("reply msgid %" PRIu64 " received:"
                   " rc=%d %zd buf bytes %zd DMA bytes\n",
                   reply.message_id, replycode, reply.message_buf_size,
                   reply.dma_buf_size);
#endif

            ASSERT_EQ(replycode,
                      testxfers[sender_xferindex].replymsg.replycode);
            ASSERT_EQ(reply.message_buf_size,
                      testxfers[sender_xferindex].replymsg.msglen);
            if (reply.message_buf) {
                ASSERT_STREQ((char *)reply.message_buf,
                             testxfers[sender_xferindex].replymsg.msgbuf);
            }

            if (reply.dma_buf_size) {
                if (strstr(
                        (char *)reply.message_buf, "DISCARD DMA") != nullptr) {
                    reply.dma_buf = malloc(reply.dma_buf_size);
                    ASSERT_NE(reply.dma_buf, nullptr);
                } else {
                    reply.dma_buf = nullptr;
                    reply.dma_buf_size = 0;
                }

                ret = sender->receiveDMA(&reply);
                ASSERT_TRUE(ret == 0);

                if (reply.dma_buf)
                    ASSERT_STREQ((char *)reply.dma_buf,
                                 testxfers[sender_xferindex].replymsg.dmabuf);

                free(reply.message_buf);
                free(reply.dma_buf);
            }
        } else {
            ret = sender->sendMessage(&msg);
            ASSERT_TRUE(ret == 0);
        }
    }
}

static void msg_test_sender(EaselComm *sender) {
    sender_done = false;

    for (int i = 0; i < kMsgTestRepeatTimes; i++) {
        msg_test_sender_iteration(sender);
    }

    {
        std::lock_guard<std::mutex> senderlock(sender_done_lock);
        sender_done = true;
        sender_done_cond.notify_one();
    }
    {
        std::unique_lock<std::mutex> receiverlock(receiver_done_lock);
        receiver_done_cond.wait(receiverlock, [&]{return receiver_done;});
    }
}

// Receiver message handler -- call receiveMessage() and process
void receiver_handle_message(EaselComm *receiver) {
    EaselComm::EaselMessage req;
    int ret;

    ret = receiver->receiveMessage(&req);
    ASSERT_TRUE(ret == 0);
    receiver_msgcount++;

#ifdef DEBUG
    printf("%s-msg msgid %" PRIu64 " received:"
           " %zd buf bytes %zd DMA bytes\n",
           req.need_reply ? "replyto" : "noreply", req.message_id,
           req.message_buf_size, req.dma_buf_size);
#endif

    // Verify message fields match the template.
    ASSERT_EQ(req.message_buf_size, testxfers[receiver_xferindex].msglen);
    if (req.message_buf_size) {
        ASSERT_STREQ((char *)req.message_buf,
                     testxfers[receiver_xferindex].msgbuf);
    }
    ASSERT_EQ(req.dma_buf_size, testxfers[receiver_xferindex].dmalen);
    if (req.dma_buf_size) {
        if (strstr((char *)req.message_buf, "DISCARD DMA") != nullptr) {
            req.dma_buf = nullptr;
            req.dma_buf_size = 0;
        } else {
            req.dma_buf = malloc(req.dma_buf_size);
            ASSERT_NE(req.dma_buf, nullptr);
        }
        ret = receiver->receiveDMA(&req);
        ASSERT_TRUE(ret == 0);

        if (req.dma_buf)
            ASSERT_STREQ((char *)req.dma_buf,
                         testxfers[receiver_xferindex].dmabuf);
    }

    free(req.message_buf);
    free(req.dma_buf);

    // Send a reply if appropriate
    if (req.need_reply) {
        ASSERT_NE(testxfers[receiver_xferindex].replymsg.msgbuf, nullptr);
        EaselComm::EaselMessage reply;
        reply.message_buf = testxfers[receiver_xferindex].replymsg.msgbuf;
        reply.message_buf_size = testxfers[receiver_xferindex].replymsg.msglen;
        reply.dma_buf = testxfers[receiver_xferindex].replymsg.dmabuf;
        reply.dma_buf_size = testxfers[receiver_xferindex].replymsg.dmalen;
        reply.need_reply = false;
        ret = receiver->sendReply(
            &req, testxfers[receiver_xferindex].replymsg.replycode, &reply);
        ASSERT_TRUE(ret == 0);
    }

    receiver_xferindex++;
    return;
}

static void msg_test_receiver(EaselComm *receiver) {
    receiver_msgcount = 0;
    receiver_done = false;

    for (int i = 0; i < kMsgTestRepeatTimes; i++) {
        receiver_xferindex = 0;

        while (receiver_xferindex < NXFERS) {
            receiver_handle_message(receiver);
        }
    }

    printf("easelcomm_test: pass complete receiver received %d messages\n",
           receiver_msgcount);
    EXPECT_EQ(receiver_msgcount, NXFERS * kMsgTestRepeatTimes);

    {
        std::lock_guard<std::mutex> receiverlock(receiver_done_lock);
        receiver_done = true;
        receiver_done_cond.notify_one();
    }
    {
        std::unique_lock<std::mutex> senderlock(sender_done_lock);
        sender_done_cond.wait(senderlock, [&]{return sender_done;});
    }
}

#ifdef EASEL_SERVER
static void test_server() {
    int ret;

    ret = easelcomm_server.open(EaselComm::EASEL_SERVICE_TEST);
    ASSERT_TRUE(ret == 0);
    easelcomm_server.flush();

    msg_test_receiver(&easelcomm_server);
    msg_test_sender(&easelcomm_server);

    easelcomm_server.close();
}
#endif // EASEL_SERVER

#ifdef AP_CLIENT

TEST(EaselCommClientTest, MessagePassingDMA) {
    int ret;
    // Let server flush before test start.
    sleep(1);

#ifdef MOCKEASEL
    // Test comm works after connect, disconnect, reconnect.
    ret = easelcomm_client.connect(NULL);
    ASSERT_TRUE(ret == 0);
    easelcomm_client.close();
    ret = easelcomm_client.connect(NULL);
    ASSERT_TRUE(ret == 0);
#endif

    ret = easelcomm_client.open(EaselComm::EASEL_SERVICE_TEST);
    ASSERT_TRUE(ret == 0);

    printf("easelcomm_test: start pass 1 client as sender\n");
    msg_test_sender(&easelcomm_client);
    printf("easelcomm_test: start pass 2 client as receiver\n");
    msg_test_receiver(&easelcomm_client);

    easelcomm_client.close();
}
#endif // AP_CLIENT

} // anonymous namespace

// --------------------------------------------------------------------------
// Test main.

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

#ifdef EASEL_SERVER
#if defined(FAKE_EASEL_TEST) || defined(MOCKEASEL)
    // Spawn thread to act as EaselComm server.
    std::thread server_thread(test_server);
#else
    test_server();
    return 0;
#endif
#endif

#ifdef AP_CLIENT
    int ret = RUN_ALL_TESTS();
    return ret;
#endif
}
