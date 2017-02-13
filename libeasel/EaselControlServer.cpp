#include <mutex>
#include <random>
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

#define NSEC_PER_SEC    1000000000ULL

namespace {
// Our EaselComm server object.  Mock uses the the network version.
#ifdef MOCKEASEL
EaselCommServerNet easel_conn;
#else
EaselCommServer easel_conn;
#endif

/*
 * Mutex protects access to gServerInitialized and easel_conn opened/closed
 * status.
 */
std::mutex gServerLock;
// true if easel_conn is opened
bool gServerInitialized;

/*
 * The AP boottime clock value we received at the last SET_TIME command,
 * converted to an nsecs_t-style count of nanoseconds, or zero if AP has not
 * sent a new value since boot or last deactivate.
 */
int64_t timesync_ap_boottime = 0;
// The local boottime clock at the time the above was set
int64_t timesync_local_boottime = 0;

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

        switch(h->command) {
        case EaselControlImpl::CMD_SET_TIME:
            {
                EaselControlImpl::SetTimeMsg *tmsg =
                    (EaselControlImpl::SetTimeMsg *)msg.message_buf;

                // Save the AP's boottime clock at approx. now
                timesync_ap_boottime = tmsg->boottime;

                // Save our current boottime time to compute deltas later
                struct timespec ts;
                if (clock_gettime(CLOCK_BOOTTIME, &ts) == 0) {
                  timesync_local_boottime =
                      (uint64_t)ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
                } else {
                  assert(0);
                    timesync_local_boottime = 0;
                }
#ifndef MOCKEASEL    // System clock should not be modified when using libmockeasel
                uint64_t timesync_ap_realtime = tmsg->realtime;
                ts.tv_sec = timesync_ap_realtime / NSEC_PER_SEC;
                ts.tv_nsec = timesync_ap_realtime - ts.tv_sec * NSEC_PER_SEC;
                if (clock_settime(CLOCK_REALTIME, &ts) != 0) {
                  assert(0);
                }
#endif
            }
            break;

        case EaselControlImpl::CMD_DEACTIVATE:
            // Invalidate current timesync value
            timesync_ap_boottime = 0;
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

/* Open our EaselCommServer object if not already. */
void initializeServer() {
    std::lock_guard<std::mutex> lk(gServerLock);

    if (gServerInitialized)
        return;

#ifdef MOCKEASEL
    easel_conn.setListenPort(EaselControlImpl::kDefaultMockSysctrlPort);
#endif
    easel_conn.open(EaselComm::EASEL_SERVICE_SYSCTRL);
    spawnIncomingMsgThread();
    gServerInitialized = true;
    return;
}

} // anonymous namespace

int EaselControlServer::open() {
    initializeServer();
    return 0;
}

void EaselControlServer::close() {
    std::lock_guard<std::mutex> lk(gServerLock);

    if (gServerInitialized) {
        easel_conn.close();
        gServerInitialized = false;
    }
}

int EaselControlServer::localToApSynchronizedClockBoottime(
        int64_t localClockval, int64_t *apSyncedClockval) {
    if (!timesync_ap_boottime)
        return -EAGAIN;

    /*
     * Return AP's base at last time sync + local delta since time of last
     * sync.
     */
    *apSyncedClockval = timesync_ap_boottime +
        (localClockval - timesync_local_boottime);
    return 0;
}

int EaselControlServer::getApSynchronizedClockBoottime(int64_t *clockval) {
    struct timespec ts;

    if (clock_gettime(CLOCK_BOOTTIME, &ts))
        return -errno;
    uint64_t now_local_boottime = (uint64_t)ts.tv_sec * NSEC_PER_SEC +
        ts.tv_nsec;

    return localToApSynchronizedClockBoottime(now_local_boottime, clockval);
}

int EaselControlServer::getLastEaselVsyncTimestamp(int64_t *timestamp) {
    /*
     * Mock version returns the current value of the sync'ed clock plus a
     * little microsecond-level fuzz just for realism.  We may need to do
     * something more realistic, such as to start with a random offset and
     * then generate 17msec deltas from that value, depending on how these
     * values will be used.
     */

    static std::default_random_engine random_generator;
    static std::uniform_int_distribution<uint64_t>
        vsync_timestamp_fuzz(-100000,100000);

    int64_t clockval;
    int ret = getApSynchronizedClockBoottime(&clockval);
    if (ret)
        return ret;
    *timestamp = clockval + vsync_timestamp_fuzz(random_generator);
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

    initializeServer();
    EaselControlImpl::LogMsg *log_msg = (EaselControlImpl::LogMsg *)buf;
    log_msg->h.command = EaselControlImpl::CMD_LOG;
    log_msg->prio = prio;
    log_msg->tag_len = tag_len;
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

/* Convenience wrapper for EaselControlServer::log() */
void easelLog(int prio, const char *tag, const char *format, ...) {
    char text[1024]; // This matches ALOG limit.

    // Write the formatted log to text.
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    EaselControlServer::log(prio, tag, text);
}
