/*
 * EaselComm unit tests.
 *
 * If compiled with AP_CLIENT defined then client-side testing code is
 * compiled;  if compiled with EASEL_SERVER defined then server-side testing
 * code is compiled.
 */

//#define DEBUG

#include "easelcomm.h"

#include "gtest/gtest.h"

#include <thread>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>

namespace {
// Number of times to repeat the message passing/DMA test sequence
const int kMsgTestRepeatTimes = 4;

#ifdef AP_CLIENT
EaselCommClient easelcomm_client;
#else
EaselCommServer easelcomm_server;
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

// Table of test message/DMA transfer templates.
// Magic strings in message text have the following meanings:
//
//    "DISCARD DMA": receiver discards the DMA transfer
//    "DYNAMIC DMA": sender generates DMA dynamically, not static from table

#define NXFERS 7
testxfer testxfers[NXFERS] = {
    { (char*)"test transfer #1 message", 25, (char*)"and a DMA buffer", 17,
    { nullptr, 0, nullptr, 0, 0} }, // no reply

    { (char*)"#2 has a message but no DMA buffer", 35, nullptr, 0,
    { nullptr, 0, nullptr, 0, 0} }, // no reply

    { (char*)"message for #3", 15, (char*)"DMA for #3", 11,
    { (char*)"reply to message #3", 20, (char*)"reply-to-#3 DMA", 16, 1040 } },

    { (char*)"#4 needs a reply and has no DMA", 31, nullptr, 0,
    { (char*)"yes it is reply to message #4", 30, nullptr, 0, 1099} },

    { (char*)"#5: DISCARD DMA", 16, (char*)"this DMA to be discarded", 25,
    { nullptr, 0, nullptr, 0, 0} }, // no reply

    { (char*)"#6 needs reply, no DMA", 23, nullptr, 0,
    { (char*)"the reply must DISCARD DMA", 27, (char*)"discard me", 11, 540} },

    { (char*)"#7 DYNAMIC DMA", 15, nullptr /*dynamic*/, 24*1024*1024 /*24MB*/,
    { nullptr, 0, nullptr, 0, 0} },
};

// Which testxfers[] entry the sender is currently working on
int sender_xferindex = 0;

// Which testxfers[] entry the receiver is currently working on
int receiver_xferindex = 0;
// How many messages the receiver processed across all iterations
int receiver_msgcount = 0;

static void msg_test_sender_iteration(EaselComm *sender) {
    int ret;

    for (sender_xferindex = 0; sender_xferindex < NXFERS; sender_xferindex++) {
        EaselComm::EaselMessage msg;
        msg.message_buf = testxfers[sender_xferindex].msgbuf;
        msg.message_buf_size = testxfers[sender_xferindex].msglen;

        if (testxfers[sender_xferindex].dmalen) {
            msg.dma_buf = malloc(testxfers[sender_xferindex].dmalen);
            ASSERT_NE(msg.dma_buf, nullptr);

            if (strstr((char *)testxfers[sender_xferindex].msgbuf,
                       "DYNAMIC DMA") != nullptr) {
                /* Generate large DMA buffer */
                uint32_t *p = (uint32_t *)msg.dma_buf;
                uint32_t i;

                for (i = 0; i < testxfers[sender_xferindex].dmalen /
                         sizeof(uint32_t); i++) {
                    *p++ = i;
                }
            } else {
                memcpy(msg.dma_buf, testxfers[sender_xferindex].dmabuf,
                       testxfers[sender_xferindex].dmalen);
            }
        } else {
            msg.dma_buf = nullptr;
        }
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

            EXPECT_EQ(replycode,
                      testxfers[sender_xferindex].replymsg.replycode);
            EXPECT_EQ(reply.message_buf_size,
                      testxfers[sender_xferindex].replymsg.msglen);
            if (reply.message_buf) {
                EXPECT_STREQ((char *)reply.message_buf,
                             testxfers[sender_xferindex].replymsg.msgbuf);
            }

            if (reply.dma_buf_size) {
                if (strstr(
                        (char *)reply.message_buf, "DISCARD DMA") != nullptr) {
                    reply.dma_buf = nullptr;
                    reply.dma_buf_size = 0;
                } else {
                    reply.dma_buf = malloc(reply.dma_buf_size);
                    ASSERT_NE(reply.dma_buf, nullptr);
                }

                ret = sender->receiveDMA(&reply);
                EXPECT_TRUE(ret == 0);

                if (reply.dma_buf)
                    EXPECT_STREQ((char *)reply.dma_buf,
                                 testxfers[sender_xferindex].replymsg.dmabuf);

                free(reply.message_buf);
                free(reply.dma_buf);
            }
        } else {
            ret = sender->sendMessage(&msg);
            EXPECT_TRUE(ret == 0);
        }

        free(msg.dma_buf);
    }
}

static void msg_test_sender(EaselComm *sender) {
    for (int i = 0; i < kMsgTestRepeatTimes; i++) {
        msg_test_sender_iteration(sender);
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
    EXPECT_EQ(req.message_buf_size, testxfers[receiver_xferindex].msglen);
    if (req.message_buf_size) {
        EXPECT_STREQ((char *)req.message_buf,
                     testxfers[receiver_xferindex].msgbuf);
    }
    EXPECT_EQ(req.dma_buf_size, testxfers[receiver_xferindex].dmalen);
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

        if (!ret && req.dma_buf) {
            if (strstr((char *)testxfers[receiver_xferindex].msgbuf,
                       "DYNAMIC DMA") != nullptr) {
                uint32_t *p = (uint32_t *)req.dma_buf;
                uint32_t i;

                for (i = 0; i < testxfers[receiver_xferindex].dmalen /
                         sizeof(uint32_t); i++) {
                    ASSERT_EQ(*p++, i);
                }
            } else {
                EXPECT_STREQ((char *)req.dma_buf,
                             testxfers[receiver_xferindex].dmabuf);
            }
        }
    }

    free(req.message_buf);
    free(req.dma_buf);

    // Send a reply if appropriate
    if (req.need_reply) {
        ASSERT_NE(testxfers[receiver_xferindex].replymsg.msgbuf, nullptr);
        EaselComm::EaselMessage reply;
        reply.message_buf = testxfers[receiver_xferindex].replymsg.msgbuf;
        reply.message_buf_size = testxfers[receiver_xferindex].replymsg.msglen;

        if (testxfers[receiver_xferindex].replymsg.dmabuf) {
            reply.dma_buf =
                malloc(testxfers[receiver_xferindex].replymsg.dmalen);
            ASSERT_NE(reply.dma_buf, nullptr);
            memcpy(reply.dma_buf, testxfers[receiver_xferindex].replymsg.dmabuf,
                   testxfers[receiver_xferindex].replymsg.dmalen);
        } else {
            reply.dma_buf = nullptr;
        }
        reply.dma_buf_size = testxfers[receiver_xferindex].replymsg.dmalen;
        reply.need_reply = false;
        ret = receiver->sendReply(
            &req, testxfers[receiver_xferindex].replymsg.replycode, &reply);
        EXPECT_TRUE(ret == 0);
        free(reply.dma_buf);
    }

    return;
}

static void msg_test_receiver(EaselComm *receiver) {
    receiver_msgcount = 0;

    for (int i = 0; i < kMsgTestRepeatTimes; i++) {
        for (receiver_xferindex = 0; receiver_xferindex < NXFERS;
             receiver_xferindex++) {
            receiver_handle_message(receiver);
        }
    }

    printf("easelcomm_test: pass complete receiver received %d messages\n",
           receiver_msgcount);
    EXPECT_EQ(receiver_msgcount, NXFERS * kMsgTestRepeatTimes);
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
    test_server();
    return 0;
#endif

#ifdef AP_CLIENT
    int ret = RUN_ALL_TESTS();
    return ret;
#endif
}
