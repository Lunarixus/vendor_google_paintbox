#define _BSD_SOURCE
#include <endian.h>
#include <thread>

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "easelcontrol.h"
#include "easelcontrol_impl.h"
#include "mockeaselcomm.h"

#define NSEC_PER_SEC    1000000000L

namespace {
// Our EaselComm server object.  Mock uses the the network version.
#ifdef MOCKEASEL
EaselCommServerNet easel_conn;
#else
EaselCommServer easel_conn;
#endif

/*
 * The AP monotonic clock value we received at the last SET_TIME command,
 * converted to an nsecs_t-style count of nanoseconds, or zero if AP has not
 * sent a new value since boot or last deactivate.
 */
int64_t timesync_ap_monotonic = 0;
// The local monotonic clock at the time the above was set
int64_t timesync_local_monotonic = 0;

// Incoming message handler thread
std::thread *msg_handler_thread;

// Handle incoming messages from EaselControlClient.
void *msgHandlerThread() {
    while (true) {
        EaselComm::EaselMessage msg;
        int ret = easel_conn.receiveMessage(&msg);
        if (ret) {
            if (errno != ESHUTDOWN)
                fprintf(stderr,
                        "easelcontrol: receiveMessage error, exiting\n");
            break;
        }

        if (msg.dma_buf_size) {
            msg.dma_buf = nullptr;
            easel_conn.receiveDMA(&msg);
        }

        if (msg.message_buf == nullptr)
            continue;

        EaselControlImpl::MsgHeader *h =
            (EaselControlImpl::MsgHeader *)msg.message_buf;

        switch(be32toh(h->command)) {
        case EaselControlImpl::CMD_SET_TIME:
            {
                EaselControlImpl::SetTimeMsg *tmsg =
                    (EaselControlImpl::SetTimeMsg *)msg.message_buf;

                // Save the AP's monotonic clock at approx. now
                timesync_ap_monotonic = be64toh(tmsg->monotonic);

                // Save our current monotonic time to compute deltas later
                struct timespec ts;
                if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
                  timesync_local_monotonic =
                      ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
                } else {
                    timesync_local_monotonic = 0;
                }
#ifndef MOCKEASEL
                // TODO(toddpoynor): Call clock_settime for REALTIME clock
                // with value tmsg->realtime on Easel.
#endif
            }
            break;

        case EaselControlImpl::CMD_DEACTIVATE:
            // Invalidate current timesync value
            timesync_ap_monotonic = 0;
            break;

        default:
            fprintf(stderr, "ERROR: unrecognized command %d\n", h->command);
            assert(0);
        }

        free(msg.message_buf);
    }

    return NULL;
}

void spawnIncomingMsgThread() {
    msg_handler_thread = new std::thread(msgHandlerThread);
}

} // anonymous namespace

int EaselControlServer::open() {
    easel_conn.setListenPort(EaselControlImpl::kDefaultMockSysctrlPort);
    easel_conn.open(EaselComm::EASEL_SERVICE_SYSCTRL);
    spawnIncomingMsgThread();
    return 0;
}

void EaselControlServer::close() {
    easel_conn.close();
}

int EaselControlServer::getApSynchronizedClockMonotonic(int64_t *clockval) {
    struct timespec ts;

    if (!timesync_ap_monotonic)
        return -EAGAIN;

    /*
     * Return AP's base at last time sync + local delta since time of last
     * sync.
     */
    if (clock_gettime(CLOCK_MONOTONIC, &ts))
        return -errno;
    uint64_t now_local_monotonic = ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
    *clockval = timesync_ap_monotonic +
        (now_local_monotonic - timesync_local_monotonic);
    return 0;
}

/*
 * Send string to client for Android log.
 * Should be identical for mock and real versions.
 */
void EaselControlServer::log(int prio, const char *tag, const char *text) {
    int log_msg_len = sizeof(EaselControlImpl::LogMsg);
    int tag_len = strlen(tag) + 1;
    int text_len = strlen(text) + 1;

    char *buf = (char *)malloc(log_msg_len + tag_len + text_len);
    if (!buf) {
        perror("malloc");
        return;
    }

    EaselControlImpl::LogMsg *log_msg = (EaselControlImpl::LogMsg *)buf;
    log_msg->h.command = htobe32(EaselControlImpl::CMD_LOG);
    log_msg->prio = htobe32(prio);
    log_msg->tag_len = htobe32(tag_len);
    memcpy(buf + log_msg_len, tag, tag_len);
    memcpy(buf + log_msg_len + tag_len, text, text_len);

    EaselComm::EaselMessage msg;
    msg.message_buf = log_msg;
    msg.message_buf_size = log_msg_len + tag_len + text_len;
    msg.dma_buf = 0;
    msg.dma_buf_size = 0;
    easel_conn.sendMessage(&msg);

    free(buf);
}
