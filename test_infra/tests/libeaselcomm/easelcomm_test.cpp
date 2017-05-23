/*
 * EaselComm unit tests.
 *
 * If compiled with AP_CLIENT defined then client-side testing code is compiled;
 * If compiled with EASEL_SERVER defined then server-side testing code is
 * compiled.
 */

// #define DEBUG

#include <inttypes.h>
#include <log/log.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <thread>

#include "easelcomm.h"

#include "gtest/gtest.h"


namespace {
// Number of times to repeat the message passing/DMA test sequence
const int kMsgTestRepeatTimes = 3;

#ifdef AP_CLIENT
    EaselCommClient easelcomm_client;
#else
    EaselCommServer easelcomm_server;
#endif

// Template for reply messages
struct testreply {
    char *msg;
    size_t msglen;
    char *dma;
    size_t dmalen;
    int replycode;
};

// Template for test message transfers
struct testxfer {
    char *msg;
    size_t msglen;
    char *dma;
    size_t dmalen;
    testreply replymsg;
};

// Table of test message/DMA transfer
// Magic strings in message text have the following meanings:
//
//    "DISCARD DMA": receiver discards the DMA transfer
//    "DYNAMIC DMA": sender generates DMA dynamically, not static from table

#define NXFERS 7
testxfer testxfers[NXFERS] = {
    {
      (char*)"test transfer #1 message", 25, (char*)"and a DMA buffer", 17,
      { nullptr, 0, nullptr, 0, 0}  // no reply
    },

    {
      (char*)"#2 has a message but no DMA buffer", 35, nullptr, 0,
      { nullptr, 0, nullptr, 0, 0}  // no reply
    },

    {
      (char*)"message for #3", 15, (char*)"DMA for #3", 11,
      { (char*)"reply to message #3", 20, (char*)"reply-to-#3 DMA", 16, 1040 }
    },

    {
      (char*)"#4 needs a reply and has no DMA", 31, nullptr, 0,
      { (char*)"yes it is reply to message #4", 30, nullptr, 0, 1099}
    },

    {
      (char*)"#5: DISCARD DMA", 16, (char*)"this DMA to be discarded", 25,
      { nullptr, 0, nullptr, 0, 0}  // no reply
    },

    {
      (char*)"#6 needs reply, no DMA", 23, nullptr, 0,
      { (char*)"the reply must DISCARD DMA", 27, (char*)"discard me", 11, 540}
    },

    {
      (char*)"#7 DYNAMIC DMA", 15, nullptr /*dynamic*/, 24*1024*1024 /*24MB*/,
      { nullptr, 0, nullptr, 0, 0}
    },
};

// testxfers[] entry the receiver is currently working on
int receiverXferIdx = 0;
// How many messages the receiver processed across all iterations
int receiverMsgCount = 0;

static void msgSenderTestIteration(EaselComm *sender) {
    for (int senderXferIdx = 0; senderXferIdx < NXFERS; senderXferIdx++) {
        EaselComm::EaselMessage msg;
        msg.message_buf = testxfers[senderXferIdx].msg;
        msg.message_buf_size = testxfers[senderXferIdx].msglen;

        if (testxfers[senderXferIdx].dmalen) {
            msg.dma_buf = malloc(testxfers[senderXferIdx].dmalen);
            ASSERT_NE(msg.dma_buf, nullptr);

            if (strstr(reinterpret_cast<char *>(testxfers[senderXferIdx].msg),
                       "DYNAMIC DMA") != nullptr) {
                /* Generate large DMA buffer */
                uint32_t *p = reinterpret_cast<uint32_t *>(msg.dma_buf);
                for (uint32_t i = 0;
                     i < testxfers[senderXferIdx].dmalen / sizeof(uint32_t);
                     i++) {
                    *p++ = i;
                }
            } else {
                memcpy(msg.dma_buf,
                       testxfers[senderXferIdx].dma,
                       testxfers[senderXferIdx].dmalen);
            }
        } else {
            msg.dma_buf = nullptr;
        }

        msg.dma_buf_size = testxfers[senderXferIdx].dmalen;
        msg.need_reply = testxfers[senderXferIdx].replymsg.msg;

        if (msg.need_reply) {
            int replycode;
            EaselComm::EaselMessage reply;
            ASSERT_EQ(sender->sendMessageReceiveReply(&msg, &replycode, &reply),
                      0);

#ifdef DEBUG
            ALOGD("reply msgid %" PRIu64 " received:"
                  " rc=%d %zd buf bytes %zd DMA bytes\n",
                  reply.message_id, replycode, reply.message_buf_size,
                  reply.dma_buf_size);
#endif

            EXPECT_EQ(replycode, testxfers[senderXferIdx].replymsg.replycode);
            EXPECT_EQ(reply.message_buf_size,
                      testxfers[senderXferIdx].replymsg.msglen);
            if (reply.message_buf) {
                EXPECT_STREQ((char *)reply.message_buf,
                             testxfers[senderXferIdx].replymsg.msg);
            }

            if (reply.dma_buf_size) {
                if (strstr(reinterpret_cast<char *>(reply.message_buf),
                           "DISCARD DMA") != nullptr) {
                    reply.dma_buf = nullptr;
                    reply.dma_buf_size = 0;
                } else {
                    reply.dma_buf = malloc(reply.dma_buf_size);
                    ASSERT_NE(reply.dma_buf, nullptr);
                }

                EXPECT_EQ(sender->receiveDMA(&reply), 0);

                if (reply.dma_buf)
                    EXPECT_STREQ((char *)reply.dma_buf,
                                 testxfers[senderXferIdx].replymsg.dma);

                free(reply.message_buf);
                free(reply.dma_buf);
            }
        } else {
            EXPECT_EQ(sender->sendMessage(&msg), 0);
        }

        free(msg.dma_buf);
    }
}

static void msgSenderTest(EaselComm *sender) {
    for (int i = 0; i < kMsgTestRepeatTimes; i++) {
        msgSenderTestIteration(sender);
    }
}

// Receiver message handler -- call receiveMessage() and process
void receiverMsgHandler(EaselComm *receiver) {
    EaselComm::EaselMessage req;
    int ret;

    ret = receiver->receiveMessage(&req);
    ASSERT_EQ(ret, 0);
    receiverMsgCount++;

#ifdef DEBUG
    ALOGD("%s-msg msgid %" PRIu64 " received:"
          " %zd buf bytes %zd DMA bytes\n",
          req.need_reply ? "replyto" : "noreply", req.message_id,
          req.message_buf_size, req.dma_buf_size);
#endif

    // Verify message fields match the template.
    EXPECT_EQ(req.message_buf_size, testxfers[receiverXferIdx].msglen);
    if (req.message_buf_size) {
        EXPECT_STREQ((char *)req.message_buf, testxfers[receiverXferIdx].msg);
    }
    EXPECT_EQ(req.dma_buf_size, testxfers[receiverXferIdx].dmalen);
    if (req.dma_buf_size) {
        if (strstr(reinterpret_cast<char *>(req.message_buf), "DISCARD DMA")
            != nullptr) {
            req.dma_buf = nullptr;
            req.dma_buf_size = 0;
        } else {
            req.dma_buf = malloc(req.dma_buf_size);
            ASSERT_NE(req.dma_buf, nullptr);
        }
        ret = receiver->receiveDMA(&req);
        ASSERT_EQ(ret, 0);

        if (!ret && req.dma_buf) {
            if (strstr(reinterpret_cast<char *>(testxfers[receiverXferIdx].msg),
                       "DYNAMIC DMA") != nullptr) {
                uint32_t *p = reinterpret_cast<uint32_t *>(req.dma_buf);
                for (uint32_t i = 0;
                     i < testxfers[receiverXferIdx].dmalen / sizeof(uint32_t);
                     i++) {
                    ASSERT_EQ(*p++, i);
                }
            } else {
                EXPECT_STREQ((char *)req.dma_buf,
                             testxfers[receiverXferIdx].dma);
            }
        }
    }

    free(req.message_buf);
    free(req.dma_buf);

    // Send a reply if appropriate
    if (req.need_reply) {
        ASSERT_NE(testxfers[receiverXferIdx].replymsg.msg, nullptr);
        EaselComm::EaselMessage reply;
        reply.message_buf = testxfers[receiverXferIdx].replymsg.msg;
        reply.message_buf_size = testxfers[receiverXferIdx].replymsg.msglen;

        if (testxfers[receiverXferIdx].replymsg.dma) {
            reply.dma_buf = malloc(testxfers[receiverXferIdx].replymsg.dmalen);
            ASSERT_NE(reply.dma_buf, nullptr);
            memcpy(reply.dma_buf,
                   testxfers[receiverXferIdx].replymsg.dma,
                   testxfers[receiverXferIdx].replymsg.dmalen);
        } else {
            reply.dma_buf = nullptr;
        }
        reply.dma_buf_size = testxfers[receiverXferIdx].replymsg.dmalen;
        reply.need_reply = false;
        ret = receiver->sendReply(
            &req, testxfers[receiverXferIdx].replymsg.replycode, &reply);
        EXPECT_EQ(ret, 0);
        free(reply.dma_buf);
    }

    return;
}

static void msgReceiverTest(EaselComm *receiver) {
    receiverMsgCount = 0;

    for (int i = 0; i < kMsgTestRepeatTimes; i++) {
        for (receiverXferIdx = 0; receiverXferIdx < NXFERS; receiverXferIdx++) {
            receiverMsgHandler(receiver);
        }
    }

    ALOGI("easelcomm_test: pass complete receiver received %d messages\n",
          receiverMsgCount);
    EXPECT_EQ(receiverMsgCount, NXFERS * kMsgTestRepeatTimes);
}

#ifdef EASEL_SERVER
static void test_server() {
    ASSERT_EQ(easelcomm_server.open(EaselComm::EASEL_SERVICE_TEST), 0);
    easelcomm_server.flush();

    msgReceiverTest(&easelcomm_server);
    msgSenderTest(&easelcomm_server);

    easelcomm_server.close();
}
#endif  // EASEL_SERVER

#ifdef AP_CLIENT
TEST(EaselCommClientTest, MessagePassing) {
    // Let server flush before test start.
    sleep(1);

    ASSERT_EQ(easelcomm_client.open(EaselComm::EASEL_SERVICE_TEST), 0);

    ALOGI("easelcomm_test: start to pass client as sender\n");
    msgSenderTest(&easelcomm_client);

    ALOGI("easelcomm_test: start to pass client as receiver\n");
    msgReceiverTest(&easelcomm_client);

    easelcomm_client.close();
}
#endif  // AP_CLIENT

}  // anonymous namespace

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
