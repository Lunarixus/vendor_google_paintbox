/*
 * EaselComm unit tests.
 *
 * If compiled with MOCKEASEL defined then TCP/IP-based mock EaselComm
 * interfaces are used.  A thread is spawned to act as the server; the client
 * connects to the service run on that thread.
 *
 * Most tests are expected to run identically on "real Easel", so long as a
 * compatible server process is run on Easel, to be addressed in the future.
 */
#include "easelcomm.h"

#ifdef MOCKEASEL
#include "mockeaselcomm.h"
#endif

#include "gtest/gtest.h"

#include <thread>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>

namespace {
#ifdef MOCKEASEL
EaselCommClientNet easelcomm_client;
EaselCommServerNet easelcomm_server;
#else
EaselCommClient easelcomm_client;
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

#define NXFERS 4
// Test message transfers
testxfer testxfers[NXFERS] = {
    { "test transfer #1 message", 25, "and a DMA buffer", 17,
    { nullptr, 0, nullptr, 0, 0} }, // no reply

    { "#2 has a message but no DMA buffer", 35, nullptr, 0,
    { nullptr, 0, nullptr, 0, 0} }, // no reply

    { "message for #3", 15, "DMA for #3", 11,
    { "reply to message #3", 19, "reply-to-#3 DMA", 16, 1040 } },

    { "#4 needs a reply and has no DMA", 31, nullptr, 0,
    { "yes it is reply to message #4", 29, nullptr, 0, 1099} },
};

// Which of the above transfers we're currently working on
int client_xferindex = 0;
int server_xferindex = 0;

// Server message handling
int doReceiveMessage() {
    EaselComm::EaselMessage req;
    int ret;

    ret = easelcomm_server.receiveMessage(&req);
    if (ret)
        return ret;

    printf("%s-msg msgid %" PRIu64 " received:"
           " %zd buf bytes %zd DMA bytes\n",
           req.need_reply ? "replyto" : "noreply", req.message_id,
           req.message_buf_size, req.dma_buf_size);

    // Verify message fields match the template.
    EXPECT_EQ(req.message_buf_size, testxfers[server_xferindex].msglen);
    if (req.message_buf_size) {
        EXPECT_STREQ((char *)req.message_buf,
                     testxfers[server_xferindex].msgbuf);
    }
    EXPECT_EQ(req.dma_buf_size, testxfers[server_xferindex].dmalen);
    if (req.dma_buf_size) {
        req.dma_buf = malloc(req.dma_buf_size);
        EXPECT_NE(req.dma_buf, nullptr);
        ret = easelcomm_server.receiveDMA(&req);
        EXPECT_TRUE(ret == 0);
        EXPECT_STREQ((char *)req.dma_buf, testxfers[server_xferindex].dmabuf);
    }

    free(req.message_buf);
    free(req.dma_buf);

    // Send a reply if appropriate
    if (req.need_reply) {
        EXPECT_NE(testxfers[server_xferindex].replymsg.msgbuf, nullptr);
        EaselComm::EaselMessage reply;
        reply.message_buf = testxfers[server_xferindex].replymsg.msgbuf;
        reply.message_buf_size = testxfers[server_xferindex].replymsg.msglen;
        reply.dma_buf = testxfers[server_xferindex].replymsg.dmabuf;
        reply.dma_buf_size = testxfers[server_xferindex].replymsg.dmalen;
        ret = easelcomm_server.sendReply(
            &req, testxfers[server_xferindex].replymsg.replycode, &reply);
        EXPECT_TRUE(ret == 0);
    }

    server_xferindex++;
    return 0;
}

} // anonymous namespace

// --------------------------------------------------------------------------
// Test client

static void run_client_tests() {
    int ret;

    for (client_xferindex = 0; client_xferindex < NXFERS; client_xferindex++) {
        EaselComm::EaselMessage msg;
        msg.message_buf = testxfers[client_xferindex].msgbuf;
        msg.message_buf_size = testxfers[client_xferindex].msglen;
        msg.dma_buf = testxfers[client_xferindex].dmabuf;
        msg.dma_buf_size = testxfers[client_xferindex].dmalen;

        if (testxfers[client_xferindex].replymsg.msgbuf) {
            int replycode;
            EaselComm::EaselMessage reply;

            ret = easelcomm_client.sendMessageReceiveReply(&msg, &replycode,
                                                           &reply);
            ASSERT_TRUE(ret == 0);

            printf("reply msgid %" PRIu64 " received:"
                   " rc=%d %zd buf bytes %zd DMA bytes\n",
                   reply.message_id, replycode, reply.message_buf_size,
                   reply.dma_buf_size);

            EXPECT_EQ(replycode,
                      testxfers[client_xferindex].replymsg.replycode);
            EXPECT_EQ(reply.message_buf_size,
                      testxfers[client_xferindex].replymsg.msglen);
            if (reply.message_buf) {
                EXPECT_STREQ((char *)reply.message_buf,
                             testxfers[client_xferindex].replymsg.msgbuf);
            }

            if (reply.dma_buf_size) {
                reply.dma_buf = malloc(reply.dma_buf_size);
                EXPECT_NE(reply.dma_buf, nullptr);
                ret = easelcomm_client.receiveDMA(&reply);
                EXPECT_TRUE(ret == 0);
                EXPECT_STREQ((char *)reply.dma_buf,
                             testxfers[client_xferindex].replymsg.dmabuf);

                free(reply.message_buf);
                free(reply.dma_buf);
            }
        } else {
          ret = easelcomm_client.sendMessage(&msg);
          ASSERT_TRUE(ret == 0);
        }
    }

    printf("easelcomm_test client sent %d messages without error\n", NXFERS);
    easelcomm_client.close();
    printf("easelcomm_test client exiting\n");
}

TEST(EaselCommClientTest, TheWholeEnchilada) {
#ifdef MOCKEASEL
    // Test comm works after connect, disconnect, reconnect.
    int ret = easelcomm_client.connect(NULL);
    ASSERT_TRUE(ret == 0);
    easelcomm_client.close();
    ret = easelcomm_client.connect(NULL);
    ASSERT_TRUE(ret == 0);
#endif

    ret = easelcomm_client.open(EaselComm::EASEL_SERVICE_TEST);
    ASSERT_TRUE(ret == 0);
    easelcomm_client.flush();

    run_client_tests();
}

// --------------------------------------------------------------------------
// Test server.  Most server tests are in message handler above.

static void test_server() {
    int ret = easelcomm_server.open(EaselComm::EASEL_SERVICE_TEST);
    ASSERT_TRUE(ret == 0);
    easelcomm_server.flush();

    while (ret == 0 && server_xferindex < NXFERS) {
        ret = doReceiveMessage();
    }

    EXPECT_EQ(server_xferindex, NXFERS);
    printf("easelcomm_test server received %d messages without error\n",
           NXFERS);
    easelcomm_server.close();
    printf("easelcomm_test server exiting\n");
}

// --------------------------------------------------------------------------
// Test main.

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

#ifdef MOCKEASEL
    // Spawn thread to act as EaselCommNet server.
    std::thread server_thread(test_server);
    sleep(1); // Let server establish socket before connecting
#endif

    int ret = RUN_ALL_TESTS();
    return ret;
}
