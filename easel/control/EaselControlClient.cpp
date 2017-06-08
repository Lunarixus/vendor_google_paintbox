#define LOG_TAG "EaselControlClient"

#include "EaselStateManager.h"
#include "EaselThermalMonitor.h"
#include "easelcontrol.h"
#include "easelcontrol_impl.h"
#include "mockeaselcomm.h"
#include "LogClient.h"

#include <thread>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unordered_map>
#include <vector>

#include <android/log.h>
#include <cutils/properties.h>
#include <log/log.h>

#define ESM_DEV_FILE    "/dev/mnh_sm"
#define NSEC_PER_SEC    1000000000ULL
#define NSEC_PER_MSEC   1000000
#define NSEC_PER_USEC   1000

namespace {
#ifdef MOCKEASEL
// Mock EaselControl uses mock EaselComm
EaselCommClientNet easel_conn;
#else
EaselCommClient easel_conn;
#endif

// EaselStateManager instance
EaselStateManager stateMgr;

// Mutex to protect EaselComm thread
std::thread *conn_thread = NULL;
std::mutex conn_mutex;
std::condition_variable conn_cond;
enum ConnState {
    CONN_STATE_CLOSED, // control channel is closed, no message thread running
    CONN_STATE_PENDING, // message thread has been started, but channel is not opened
    CONN_STATE_OPENED, // channel is opened and handshake has completed successfully
    CONN_STATE_FAILED, // channel closed because of some failure
} conn_state = CONN_STATE_CLOSED;

EaselLog::LogClient gLogClient;

// Mutex to protect the current state of EaselControlClient
std::mutex state_mutex;
enum ControlState {
    INIT,        // Unknown initial state
    SUSPENDED,   // Suspended
    RESUMED,     // Powered, but no EaselCommClient channel
    ACTIVATED,   // Powered, EaselCommClient channel is active
} state = INIT;

// EaselThermalMonitor instance
EaselThermalMonitor thermalMonitor;
static const std::vector<struct EaselThermalMonitor::Configuration> thermalCfg = {
    {"bcm15602_tz", 1},
    {"fpc_therm", 1000},
    {"back_therm", 1000},
    {"pa_therm", 1000},
    {"msm_therm", 1000},
    {"quiet_therm", 1000},
};

/*
 * Handle incoming messages from EaselControlServer.
 */
void handleMessages() {
    while (true) {
        EaselComm::EaselMessage msg;
        int ret = easel_conn.receiveMessage(&msg);
        if (ret) {
            if (errno != ESHUTDOWN) {
                ALOGE("easelcontrol: receiveMessage error (%d, %d), exiting\n", ret, errno);
            }
            break;
        }

        if (msg.message_buf == nullptr) {
            continue;
        }

        EaselControlImpl::MsgHeader *h =
            (EaselControlImpl::MsgHeader *)msg.message_buf;

        switch (h->command) {
            // Currently there is no messages expected to be received here.
        default:
            ALOGE("easelcontrol: unknown command code %d received\n",
                    h->command);
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

static int sendTimestamp(void) {
    int ret;

    ALOGD("%s\n", __FUNCTION__);

    // Prepare local timestamp and send to server
    EaselControlImpl::SetTimeMsg ctrl_msg;
    ctrl_msg.h.command = EaselControlImpl::CMD_SET_TIME;
    struct timespec ts ;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    ctrl_msg.boottime = (uint64_t)ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
    clock_gettime(CLOCK_REALTIME, &ts);
    ctrl_msg.realtime = (uint64_t)ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;

    EaselComm::EaselMessage msg;
    msg.message_buf = &ctrl_msg;
    msg.message_buf_size = sizeof(ctrl_msg);
    msg.dma_buf = 0;
    msg.dma_buf_size = 0;
    msg.need_reply = true;

    int replycode;
    EaselComm::EaselMessage reply;

    ret = easel_conn.sendMessageReceiveReply(&msg, &replycode, &reply);
    if (ret) {
        ALOGE("%s: Failed to send timestamp (%d)\n", __FUNCTION__, ret);
        return ret;
    }

    if (replycode != EaselControlImpl::REPLY_SET_TIME_OK) {
        ALOGE("%s: Failed to receive SET_TIME_OK (%d)\n", __FUNCTION__, replycode);
        return ret;
    }

    // Get timestamp returned by server
    EaselControlImpl::SetTimeMsg *tmsg =
                (EaselControlImpl::SetTimeMsg *)reply.message_buf;

    // Check local timestamp again
    struct timespec new_ts;
    clock_gettime(CLOCK_REALTIME, &new_ts);
    uint64_t realtime = (uint64_t)new_ts.tv_sec * NSEC_PER_SEC + new_ts.tv_nsec;

    ALOGD("%s: Server timestamp is %ld us behind (oneway)\n" , __FUNCTION__,
          ((long)realtime - (long)tmsg->realtime) / NSEC_PER_USEC);
    ALOGD("%s took %ld us\n" , __FUNCTION__,
          ((long)realtime - (long)ctrl_msg.realtime) / NSEC_PER_USEC);

    return ret;
}

int sendActivateCommand()
{
    EaselControlImpl::ActivateMsg ctrl_msg;
    EaselComm::EaselMessage msg;
    int ret = 0;

    ctrl_msg.h.command = EaselControlImpl::CMD_ACTIVATE;

    struct timespec ts ;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    ctrl_msg.boottime = (uint64_t)ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
    clock_gettime(CLOCK_REALTIME, &ts);
    ctrl_msg.realtime = (uint64_t)ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;

    msg.message_buf = &ctrl_msg;
    msg.message_buf_size = sizeof(ctrl_msg);
    msg.dma_buf = 0;
    msg.dma_buf_size = 0;
    msg.need_reply = true;

    int replycode;

    ret = easel_conn.sendMessageReceiveReply(&msg, &replycode, nullptr);
    if (ret) {
        ALOGE("%s: Failed to send activate message to Easel (%d)\n", __FUNCTION__, ret);
        return ret;
    }

    if (replycode != EaselControlImpl::REPLY_ACTIVATE_OK) {
        ALOGE("%s: Failed to receive ACTIVATE_OK (%d)\n", __FUNCTION__, replycode);
        return ret;
    }

    ret = sendTimestamp();
    if (ret) {
        ALOGE("%s: Failed to send sendTimestamp (%d)\n", __FUNCTION__, ret);
        return ret;
    }

    return 0;
}

int sendDeactivateCommand()
{
    EaselControlImpl::DeactivateMsg ctrl_msg;
    EaselComm::EaselMessage msg;
    int ret = 0;

    ctrl_msg.h.command = EaselControlImpl::CMD_DEACTIVATE;

    msg.message_buf = &ctrl_msg;
    msg.message_buf_size = sizeof(ctrl_msg);
    msg.dma_buf = 0;
    msg.dma_buf_size = 0;
    ret = easel_conn.sendMessage(&msg);
    if (ret) {
        ALOGE("%s: failed to send deactivate command to Easel (%d)\n", __FUNCTION__, ret);
    }

    return ret;
}

void setConnStateAndNotify(enum ConnState state)
{
    {
        std::lock_guard<std::mutex> conn_lock(conn_mutex);
        conn_state = state;
    }
    conn_cond.notify_all();
}

void easelConnThread()
{
    int ret;

    // Wait for state manager to reach ACTIVE state, which means that Easel is
    // powered and is executing firmware. This is separate from Activated state,
    // which means EaselControlServer is running in HDR+ mode.
    ALOGD("%s: Waiting for active state", __FUNCTION__);
    ret = stateMgr.waitForState(EaselStateManager::ESM_STATE_ACTIVE);
    if (ret) {
        ALOGE("%s: Easel failed to enter active state (%d)\n", __FUNCTION__, ret);
        setConnStateAndNotify(CONN_STATE_FAILED);
        return;
    }

    ALOGI("%s: Opening easel_conn", __FUNCTION__);
    ret = easel_conn.open(EaselComm::EASEL_SERVICE_SYSCTRL);
    if (ret) {
        ALOGE("%s: Failed to open easelcomm connection (%d)",
              __FUNCTION__, ret);
        setConnStateAndNotify(CONN_STATE_FAILED);
        return;
    }

#ifndef MOCKEASEL
    ALOGI("%s: waiting for handshake\n", __FUNCTION__);
    ret = easel_conn.initialHandshake();
    if (ret) {
        ALOGE("%s: Failed to handshake with server", __FUNCTION__);
        setConnStateAndNotify(CONN_STATE_FAILED);
        return;
    }
    ALOGI("%s: handshake done\n", __FUNCTION__);
#endif

    if (!property_get_int32("persist.camera.hdrplus.enable", 0)) {
        EaselControlImpl::DeactivateMsg ctrl_msg;

        ctrl_msg.h.command = EaselControlImpl::CMD_DEACTIVATE;

        EaselComm::EaselMessage msg;
        msg.message_buf = &ctrl_msg;
        msg.message_buf_size = sizeof(ctrl_msg);
        msg.dma_buf = 0;
        msg.dma_buf_size = 0;
        ret = easel_conn.sendMessage(&msg);
        if (ret) {
            ALOGE("%s: failed to send deactivate command to Easel (%d)\n", __FUNCTION__, ret);
        }
    }

    setConnStateAndNotify(CONN_STATE_OPENED);

    handleMessages();

    setConnStateAndNotify(CONN_STATE_CLOSED);
}

int setupEaselConn()
{
    {
        std::unique_lock<std::mutex> conn_lock(conn_mutex);
        if (conn_state == CONN_STATE_OPENED) {
            return 0;
        }
    }

    if (conn_thread == NULL) {
        {
            std::unique_lock<std::mutex> conn_lock(conn_mutex);
            conn_state = CONN_STATE_PENDING;
        }

        conn_thread = new std::thread(easelConnThread);
        if (conn_thread == NULL) {
            return -ENOMEM;
        }

        return 0;
    }

    {
        std::unique_lock<std::mutex> conn_lock(conn_mutex);
        conn_cond.wait(conn_lock, []{return (conn_state != CONN_STATE_PENDING);});
        if (conn_state == CONN_STATE_FAILED) {
            ALOGE("%s: Resume failed because of easelConnThread failure", __FUNCTION__);
            return -EIO;
        }
    }

    return 0;
}

int teardownEaselConn()
{
    if (!conn_thread) {
        return 0;
    }

    {
        std::unique_lock<std::mutex> conn_lock(conn_mutex);
        if (conn_state == CONN_STATE_CLOSED) {
            return 0;
        } else if (conn_state == CONN_STATE_PENDING) {
            conn_cond.wait(conn_lock, []{return (conn_state != CONN_STATE_PENDING);});
        }
    }

    easel_conn.close();

    {
        std::unique_lock<std::mutex> conn_lock(conn_mutex);
        if (conn_state == CONN_STATE_OPENED) {
            conn_cond.wait(conn_lock, []{return (conn_state != CONN_STATE_OPENED);});
        }
    }

    conn_thread->join();
    delete conn_thread;
    conn_thread = NULL;

    return 0;
}

int startThermalMonitor()
{
    int ret = thermalMonitor.start();
    if (ret) {
        ALOGE("failed to start EaselThermalMonitor (%d)\n", ret);
    }

    return ret;
}

int stopThermalMonitor()
{
    int ret = thermalMonitor.stop();
    if (ret) {
        ALOGE("%s: failed to stop EaselThermalMonitor (%d)\n", __FUNCTION__, ret);
    }

    return ret;
}

int startLogClient()
{
    int ret = gLogClient.start();
    if (ret) {
        ALOGE("Failed to start LogClient (%d)\n", ret);
    }

    return ret;
}

int stopLogClient()
{
    gLogClient.stop();

    return 0;
}

int switchState(enum ControlState nextState)
{
    int ret = 0;

    std::unique_lock<std::mutex> state_lock(state_mutex);

    ALOGD("%s: Switch from state %d to state %d", __FUNCTION__, state, nextState);

    if (state == nextState) {
        return 0;
    }

    switch (nextState) {
        case ControlState::SUSPENDED: {
            switch (state) {
                case ControlState::ACTIVATED:
                    ret = sendDeactivateCommand();
                    ret |= stopThermalMonitor();
                    ret |= stopLogClient();
                    ret |= teardownEaselConn();
                    ret |= stateMgr.setState(EaselStateManager::ESM_STATE_OFF);
                    break;
                case ControlState::RESUMED:
                case ControlState::INIT:
                    ret |= stopThermalMonitor();
                    ret |= stopLogClient();
                    ret |= teardownEaselConn();
                    ret |= stateMgr.setState(EaselStateManager::ESM_STATE_OFF);
                    break;
                default:
                    ALOGE("%s: Invalid state transition from %d to %d", __FUNCTION__, state,
                          nextState);
                    ret = -EINVAL;
                    break;
            }
            break;
        }

        case ControlState::RESUMED: {
            switch (state) {
                case ControlState::SUSPENDED:
                    ret = stateMgr.setState(EaselStateManager::ESM_STATE_ACTIVE, false);
                    if (!ret) {
                        ret = setupEaselConn();
                    }
                    if (!ret) {
                        ret = startLogClient();
                    }
                    if (!ret) {
                        ret = startThermalMonitor();
                    }
                    break;
                case ControlState::ACTIVATED:
                    ret = sendDeactivateCommand();
                    break;
                default:
                    ALOGE("%s: Invalid state transition from %d to %d", __FUNCTION__, state,
                          nextState);
                    ret = -EINVAL;
                    break;
            }
            break;
        }

        case ControlState::ACTIVATED: {
            switch (state) {
                case ControlState::SUSPENDED:
                    ret = stateMgr.setState(EaselStateManager::ESM_STATE_ACTIVE, false);
                    if (!ret) {
                        ret = setupEaselConn();
                        if (!ret) {
                            ret = startLogClient();
                        }
                        if (!ret) {
                            ret = startThermalMonitor();
                        }
                        if (!ret) {
                            ret = sendActivateCommand();
                        }
                    }
                    break;
                case ControlState::RESUMED:
                    ret = setupEaselConn();
                    if (!ret) {
                        ret = sendActivateCommand();
                    }
                    break;
                default:
                    ALOGE("%s: Invalid state transition from %d to %d", __FUNCTION__, state,
                          nextState);
                    ret = -EINVAL;
                    break;
            }
            break;
        }

        default:
            ALOGE("%s: Invalid nextState %d", __FUNCTION__, nextState);
            ret = -EINVAL;
            break;
    }

    if (ret) {
        ALOGE("%s: Failed to switch from state %d to state %d (%d)", __FUNCTION__, state,
              nextState, ret);
    } else {
        state = nextState;
    }

    return ret;
}

int EaselControlClient::activate() {
    int ret = 0;

    ALOGI("%s\n", __FUNCTION__);

    ret = switchState(ControlState::ACTIVATED);
    if (ret) {
        ALOGE("%s: failed to activate Easel (%d)\n", __FUNCTION__, ret);
    }

    return 0;
}

int EaselControlClient::deactivate() {
    int ret = 0;

    ALOGI("%s\n", __FUNCTION__);

    ret = switchState(ControlState::RESUMED);
    if (ret) {
        ALOGE("%s: failed to deactivate Easel (%d)\n", __FUNCTION__, ret);
    }

    return ret;
}

int EaselControlClient::startMipi(enum EaselControlClient::Camera camera, int rate,
                                  bool enableIpu)
{
    struct EaselStateManager::EaselMipiConfig config = {
        .rxRate = rate, .txRate = rate,
    };
    int ret;

    ALOGI("%s: camera %d, rate %d, enableIpu %d\n", __FUNCTION__,
          camera, rate, enableIpu);

    if (enableIpu) {
        config.mode = EaselStateManager::EaselMipiConfig::ESL_MIPI_MODE_BYPASS_W_IPU;
    } else {
        config.mode = EaselStateManager::EaselMipiConfig::ESL_MIPI_MODE_BYPASS;
    }
    if (camera == EaselControlClient::MAIN) {
        config.rxChannel = EaselStateManager::EaselMipiConfig::ESL_MIPI_RX_CHAN_0;
        config.txChannel = EaselStateManager::EaselMipiConfig::ESL_MIPI_TX_CHAN_0;
    } else {
        config.rxChannel = EaselStateManager::EaselMipiConfig::ESL_MIPI_RX_CHAN_1;
        config.txChannel = EaselStateManager::EaselMipiConfig::ESL_MIPI_TX_CHAN_1;
    }

    ret = stateMgr.waitForPower();
    if (ret) {
        ALOGE("Could not start MIPI because Easel is not powered (%d)\n", ret);
        return ret;
    }

    return stateMgr.startMipi(&config);
}

int EaselControlClient::stopMipi(enum EaselControlClient::Camera camera)
{
    struct EaselStateManager::EaselMipiConfig config;

    ALOGD("%s: camera %d\n", __FUNCTION__, camera);

    if (camera == EaselControlClient::MAIN) {
        config.rxChannel = EaselStateManager::EaselMipiConfig::ESL_MIPI_RX_CHAN_0;
        config.txChannel = EaselStateManager::EaselMipiConfig::ESL_MIPI_TX_CHAN_0;
    } else {
        config.rxChannel = EaselStateManager::EaselMipiConfig::ESL_MIPI_RX_CHAN_1;
        config.txChannel = EaselStateManager::EaselMipiConfig::ESL_MIPI_TX_CHAN_1;
    }

    return stateMgr.stopMipi(&config);
}

// Called when the camera app is opened
int EaselControlClient::resume() {
    int ret = 0;

    ALOGD("%s\n", __FUNCTION__);

    ret = switchState(ControlState::RESUMED);
    if (ret) {
        ALOGE("Failed to resume Easel (%d)\n", ret);
    }

    return ret;
}

// Called when the camera app is closed
int EaselControlClient::suspend() {
    int ret = 0;

    ALOGD("%s\n", __FUNCTION__);

    ret = switchState(ControlState::SUSPENDED);
    if (ret) {
        ALOGE("%s: failed to suspend Easel (%d)\n", __FUNCTION__, ret);
    }

    return ret;
}

int EaselControlClient::open() {
    int ret = 0;

    ALOGD("%s\n", __FUNCTION__);

    ret = thermalMonitor.open(thermalCfg);
    if (ret) {
        ALOGE("failed to open EaselThermalMonitor (%d)\n", ret);
        return ret;
    }

    ret = stateMgr.open();
    if (ret) {
        ALOGE("failed to initialize EaselStateManager (%d)\n", ret);
        return ret;
    }

    ret = switchState(ControlState::SUSPENDED);
    if (ret) {
        ALOGE("%s: failed to suspend Easel (%d)\n", __FUNCTION__, ret);
    }

    return ret;
}

// Temporary for the TCP/IP-based mock
#ifdef MOCKEASEL
int EaselControlClient::open(const char *easelhost) {
    int ret = easel_conn.connect(easelhost,
                                 EaselControlImpl::kDefaultMockSysctrlPort);
    if (ret)
        return ret;
    return open();
}
#endif

void EaselControlClient::close() {
    int ret;

    ret = switchState(ControlState::SUSPENDED);
    if (ret) {
        ALOGE("%s: failed to suspend Easel (%d)\n", __FUNCTION__, ret);
    }

    {
        std::lock_guard<std::mutex> state_lock(state_mutex);
        state = ControlState::INIT;
    }


    stateMgr.close();
    thermalMonitor.close();
}

bool isEaselPresent()
{
    bool ret = false;
    int fd = open(ESM_DEV_FILE, O_RDONLY);

    if (fd >= 0) {
        ret = true;
        close(fd);
    }

    return ret;
}
