#define LOG_TAG "EaselControlClient"

#define _BSD_SOURCE
#include <endian.h>

#include "easelcontrol.h"
#include "easelcontrol_impl.h"
#include "mockeaselcomm.h"

#include <thread>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef ANDROID
#include <android/log.h>
#endif
#ifdef ANDROID
#include <utils/Log.h>
#endif

#define NSEC_PER_SEC    1000000000ULL

namespace {
#ifdef MOCKEASEL
// Mock EaselControl uses mock EaselComm
EaselCommClientNet easel_conn;
#else
EaselCommClient easel_conn;
#endif

// Incoming message handler thread
std::thread *msg_handler_thread;

/*
 * Handle CMD_LOG Android logging control message received from server.
 */
void handleLog(const EaselControlImpl::LogMsg *msg) {
    char *tag = (char *)msg + sizeof(EaselControlImpl::LogMsg);
    char *text = tag + be32toh(msg->tag_len);
#ifdef ANDROID
    __android_log_write(be32toh(msg->prio), tag, text);
#else
    printf("<%d> %s %s\n", be32toh(msg->prio), tag, text);
#endif
}

/*
 * Handle incoming messages from EaselControlServer.
 */
void msgHandlerThread() {
    while (true) {
        EaselComm::EaselMessage msg;
        int ret = easel_conn.receiveMessage(&msg);
        if (ret) {
            if (errno != ESHUTDOWN)
#ifdef ANDROID
                ALOGI("easelcontrol: receiveMessage error, exiting\n");
#else
                fprintf(stderr,
                        "easelcontrol: receiveMessage error, exiting\n");
#endif
            break;
        }

        if (msg.message_buf == nullptr)
            continue;

        EaselControlImpl::MsgHeader *h =
            (EaselControlImpl::MsgHeader *)msg.message_buf;

        switch (be32toh(h->command)) {
        case EaselControlImpl::CMD_LOG:
            handleLog((EaselControlImpl::LogMsg *)msg.message_buf);
            break;
        default:
#ifdef ANDROID
            ALOGE("easelcontrol: unknown command code %d received\n",
                    h->command);
#else
            fprintf(stderr, "easelcontrol: unknown command code %d received\n",
                    h->command);
#endif
            // TODO(toddpoynor): panic restart Easel on misbehavior
        }

        /*
         * DMA transfers are never requested by EaselControl, but just in
         * case, throw away any DMA buffer requested.
         */
        if (msg.dma_buf_size) {
            msg.dma_buf = nullptr;
            easel_conn.receiveDMA(&msg);
        }

        free(msg.message_buf);
    }
}

} // anonymous namespace

int EaselControlClient::activateEasel() {
    /*
     * Send a message with the new boottime base and time of day clock.
     */
    EaselControlImpl::SetTimeMsg ctrl_msg;
    ctrl_msg.h.command = htobe32(EaselControlImpl::CMD_SET_TIME);
    struct timespec ts ;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    ctrl_msg.boottime = htobe64((uint64_t)ts.tv_sec * NSEC_PER_SEC +
                                ts.tv_nsec);
    clock_gettime(CLOCK_REALTIME, &ts);
    ctrl_msg.realtime = htobe64((uint64_t)ts.tv_sec * NSEC_PER_SEC +
                                ts.tv_nsec);

    EaselComm::EaselMessage msg;
    msg.message_buf = &ctrl_msg;
    msg.message_buf_size = sizeof(ctrl_msg);
    msg.dma_buf = 0;
    msg.dma_buf_size = 0;
    return easel_conn.sendMessage(&msg);
}

int EaselControlClient::deactivateEasel() {
    EaselControlImpl::DeactivateMsg ctrl_msg;
    ctrl_msg.h.command = htobe32(EaselControlImpl::CMD_DEACTIVATE);

    EaselComm::EaselMessage msg;
    msg.message_buf = &ctrl_msg;
    msg.message_buf_size = sizeof(ctrl_msg);
    msg.dma_buf = 0;
    msg.dma_buf_size = 0;
    return easel_conn.sendMessage(&msg);
}

int EaselControlClient::open() {
    int ret = easel_conn.open(EaselComm::EASEL_SERVICE_SYSCTRL);
    if (ret)
        return ret;
    msg_handler_thread = new std::thread(msgHandlerThread);
    return 0;
}

// Temporary for the TCP/IP-based mock
#ifdef MOCKEASEL
int EaselControlClient::open(const char *easelhost) {
    int ret = easel_conn.connect(easelhost ? easelhost : "localhost",
                                 EaselControlImpl::kDefaultMockSysctrlPort);
    if (ret)
        return ret;
    return open();
}
#endif

void EaselControlClient::close() {
    easel_conn.close();
}
