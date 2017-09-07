#define LOG_TAG "EaselControlServer"

#include <mutex>
#include <random>
#include <thread>

#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <log/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "EaselClockControl.h"
#include "EaselThermalMonitor.h"
#include "easelcontrol.h"
#include "easelcontrol_impl.h"
#include "easelcomm.h"

#define NSEC_PER_SEC    1000000000ULL

// Compensate +700 us to timestamp (arbitrary value based on experiments)
#define ADJUSTED_TIMESTAMP_LATENCY_NS   700000ULL

// sysfs file to initiate kernel suspend
#define KERNEL_SUSPEND_SYS_FILE    "/sys/power/state"
#define KERNEL_SUSPEND_STRING      "mem"

// sysfs files that contain wafer information
#define WAFER_UPPER_PATH  "/sys/firmware/devicetree/base/chosen/wafer_upper"
#define WAFER_LOWER_PATH  "/sys/firmware/devicetree/base/chosen/wafer_lower"

namespace {
EaselCommServer easel_conn;

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

// EaselThermalMonitor instance
EaselThermalMonitor thermalMonitor;
static const std::vector<struct EaselThermalMonitor::Configuration> thermalCfg = {
    {"lpddr", 1, {65000, 75000, 85000}},
    {"cpu",   1, {65000, 75000, 85000}},
    {"ipu1",  1, {65000, 75000, 85000}},
    {"ipu2",  1, {65000, 75000, 85000}},
};

void setTimeFromMsg(uint64_t boottime, uint64_t realtime)
{
    // Save the AP's boottime clock at approx. now
    timesync_ap_boottime = boottime;
    timesync_ap_boottime += ADJUSTED_TIMESTAMP_LATENCY_NS;

    // Save our current boottime time to compute deltas later
    struct timespec ts;
    if (clock_gettime(CLOCK_BOOTTIME, &ts) == 0) {
      timesync_local_boottime =
          (uint64_t)ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
    } else {
      assert(0);
        timesync_local_boottime = 0;
    }
    uint64_t timesync_ap_realtime = realtime + ADJUSTED_TIMESTAMP_LATENCY_NS;
    ts.tv_sec = timesync_ap_realtime / NSEC_PER_SEC;
    ts.tv_nsec = timesync_ap_realtime - ts.tv_sec * NSEC_PER_SEC;
    if (clock_settime(CLOCK_REALTIME, &ts) != 0) {
      assert(0);
    }
}

// Handle incoming messages from EaselControlClient.
void msgHandlerCallback(EaselComm::EaselMessage* msg) {
    EaselControlImpl::MsgHeader *h =
        (EaselControlImpl::MsgHeader *)msg->message_buf;

    ALOGI("Received command %d", h->command);

    int ret = 0;

    switch(h->command) {
    case EaselControlImpl::CMD_ACTIVATE: {
      EaselControlServer::setClockMode(EaselControlServer::ClockMode::Functional);

        // Server will not set realtime on receiving CMD_ACTIVATE
        // Instead, we assume client will send another CMD_SET_TIME after
        // receiving this reply.
        easel_conn.sendReply(msg, EaselControlImpl::REPLY_ACTIVATE_OK, nullptr);

        ret = thermalMonitor.start();
        if (ret) {
            ALOGE("failed to start EaselThermalMonitor (%d)", ret);
        }

        break;
    }

    case EaselControlImpl::CMD_DEACTIVATE: {
        // Invalidate current timesync value
        timesync_ap_boottime = 0;

        ret = thermalMonitor.stop();
        if (ret) {
            ALOGE("%s: failed to stop EaselThermalMonitor (%d)", __FUNCTION__, ret);
        }

        EaselControlServer::setClockMode(EaselControlServer::ClockMode::Bypass);
        break;
    }

    case EaselControlImpl::CMD_SUSPEND: {
        // Send command to suspend the kernel
        int fd = open(KERNEL_SUSPEND_SYS_FILE, O_WRONLY);
        char buf[] = KERNEL_SUSPEND_STRING;

        if (fd >= 0) {
            write(fd, buf, strlen(buf));
            close(fd);
        } else {
            ALOGE("easelcontrol: could not open power management sysfs file");
        }

        break;
    }

    case EaselControlImpl::CMD_SET_TIME: {
        EaselControlImpl::SetTimeMsg *tmsg =
            (EaselControlImpl::SetTimeMsg *)msg->message_buf;

        setTimeFromMsg(tmsg->boottime, tmsg->realtime);

        // Send server timestamp back to client
        EaselControlImpl::SetTimeMsg reply_msg;
        reply_msg.h.command = EaselControlImpl::CMD_SET_TIME;
        struct timespec ts ;
        clock_gettime(CLOCK_BOOTTIME, &ts);
        reply_msg.boottime = (uint64_t)ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
        clock_gettime(CLOCK_REALTIME, &ts);
        reply_msg.realtime = (uint64_t)ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;

        EaselComm::EaselMessage reply;
        reply.message_buf = &reply_msg;
        reply.message_buf_size = sizeof(reply_msg);
        reply.dma_buf = 0;
        reply.dma_buf_size = 0;

        easel_conn.sendReply(msg, EaselControlImpl::REPLY_SET_TIME_OK, &reply);
        break;
    }

    default:
        ALOGE("ERROR: unrecognized command %d", h->command);
        assert(0);
    }
}

/* Open our EaselCommServer object if not already. */
int initializeServer() {
    int ret = 0;
    std::lock_guard<std::mutex> lk(gServerLock);

    if (gServerInitialized)
        return ret;

    ret = easel_conn.open(EaselComm::EASEL_SERVICE_SYSCTRL);
    if (ret) {
        ALOGE("%s: failed to open easel_conn (%d)", __FUNCTION__, ret);
        return ret;
    }

    ret = easel_conn.initialHandshake();
    if (ret) {
        ALOGE("%s: Failed to handshake with client (%d)", __FUNCTION__, ret);
        return ret;
    }

    easel_conn.startMessageHandlerThread(msgHandlerCallback);
    gServerInitialized = true;
    return ret;
}

void printWaferId()
{
    // Send command to suspend the kernel
    FILE *fp = fopen(WAFER_LOWER_PATH, "rb");
    char buf[8];
    uint32_t upper, lower;

    if (!fp) {
        return;
    }

    if (fread(buf, 1, 4, fp) < 4) {
        fclose(fp);
        return;
    }
    fclose(fp);

    fp = fopen(WAFER_UPPER_PATH, "rb");
    if (!fp) {
        return;
    }

    if (fread(buf+4, 1, 4, fp) < 4) {
        fclose(fp);
        return;
    }
    fclose(fp);

    upper = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
    lower = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];

    ALOGI("%s: Wafer Id 0x%08x%08x", __FUNCTION__, upper, lower);
}

} // anonymous namespace

int EaselControlServer::open() {
    int ret;

    ret = initializeServer();
    if (ret) {
        return ret;
    }

    ret = thermalMonitor.open(thermalCfg);
    if (ret) {
        ALOGE("failed to open EaselThermalMonitor (%d)", ret);
        return ret;
    }

    printWaferId();

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

EaselControlServer::ThermalCondition EaselControlServer::setClockMode(ClockMode mode)
{
    enum EaselThermalMonitor::Condition condition = thermalMonitor.getCondition();
    EaselClockControl::setMode((EaselClockControl::Mode)mode, condition);
    return (EaselControlServer::ThermalCondition)condition;
}

EaselControlServer::ClockMode EaselControlServer::getClockMode()
{
    return (EaselControlServer::ClockMode)EaselClockControl::getMode();
}

bool EaselControlServer::isNewThermalCondition()
{
    return EaselClockControl::isNewThermalCondition(thermalMonitor.getCondition());
}

EaselControlServer::ThermalCondition EaselControlServer::getThermalCondition()
{
  return (EaselControlServer::ThermalCondition)thermalMonitor.getCondition();
}
