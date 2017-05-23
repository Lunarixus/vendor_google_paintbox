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

// Lock to guard gCallbackMap
std::mutex gCallbackMapMutex;
// Map from callbackId to callback functions.
std::unordered_map<int, std::function<void(const ControlData &)>> gCallbackMap;

// Mutex to protect EaselComm thread
std::thread *conn_thread = NULL;
std::mutex conn_mutex;
std::condition_variable conn_cond;
bool conn_ready = false;

EaselLog::LogClient gLogClient;

// Mutex to protect the current state of EaselControlClient
std::mutex state_mutex;
enum ControlState {
    SUSPENDED,   // Suspended
    RESUMED,     // Powered, but no EaselCommClient channel
    ACTIVATED,   // Powered, EaselCommClient channel is active
} state = SUSPENDED;

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
        case EaselControlImpl::CMD_RPC: {
            EaselControlImpl::RpcMsg *rpcMsg =
                    (EaselControlImpl::RpcMsg *)msg.message_buf;
            {
                std::lock_guard<std::mutex> lock(gCallbackMapMutex);

                if (rpcMsg->callbackId == 0) {
                    ALOGE("Callback id is empty from server RpcMsg");
                    break;
                }

                if (gCallbackMap.count(rpcMsg->callbackId) == 0) {
                    ALOGE("callback id %" PRIu64 " not found.", rpcMsg->callbackId);
                    break;
                }

                ControlData response(rpcMsg->payloadBody, rpcMsg->payloadSize);

                // Response size should have been checked on server side
                if(response.size > EaselControlImpl::kMaxPayloadSize) {
                    ALOGE("Response size out of boundary %" PRIu64, response.size);
                    break;
                }

                gCallbackMap[rpcMsg->callbackId](response);
                gCallbackMap.erase(rpcMsg->callbackId);
            }
            break;
        }
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

void easelConnThread()
{
    int ret;

    ALOGI("%s: Opening easel_conn", __FUNCTION__);
    ret = easel_conn.open(EaselComm::EASEL_SERVICE_SYSCTRL);
    if (ret) {
        ALOGE("%s: Failed to open easelcomm connection (%d)",
              __FUNCTION__, ret);
        return;
    }

#ifndef MOCKEASEL
    ALOGI("%s: waiting for handshake\n", __FUNCTION__);
    ret = easel_conn.initialHandshake();
    if (ret) {
        ALOGE("%s: Failed to handshake with server", __FUNCTION__);
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

    std::unique_lock<std::mutex> conn_lock(conn_mutex);
    conn_ready = true;
    conn_lock.unlock();

    conn_cond.notify_one();

    handleMessages();
}

int setupEaselConn()
{
    std::unique_lock<std::mutex> conn_lock(conn_mutex);

    if (conn_ready) {
        return 0;
    }

    if (conn_thread == NULL) {
        conn_thread = new std::thread(easelConnThread);
        if (conn_thread == NULL) {
            return -ENOMEM;
        }
        return 0;
    }

    conn_cond.wait(conn_lock, []{return conn_ready;});

    return 0;
}

int teardownEaselConn()
{
    std::unique_lock<std::mutex> conn_lock(conn_mutex);

    if (!conn_thread) {
        return 0;
    }

    if (!conn_ready) {
        conn_cond.wait(conn_lock, []{return conn_ready;});
    }

    easel_conn.close();

    conn_thread->join();
    delete conn_thread;
    conn_thread = NULL;
    conn_ready = false;

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
                    ret |= teardownEaselConn();
                    ret |= stateMgr.setState(EaselStateManager::ESM_STATE_OFF);
                    break;
                case ControlState::RESUMED:
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

void getRpcMsg(
        int handlerId,
        int rpcId,
        bool callback,
        const ControlData &payload,
        EaselControlImpl::RpcMsg *msg) {
    static uint64_t callbackId = 0;

    msg->handlerId = handlerId;
    msg->rpcId = rpcId;
    if (callback) {
        callbackId++;
        msg->callbackId = callbackId;
    } else {
        msg->callbackId = 0;
    }
    msg->payloadSize = payload.size;
    memcpy(msg->payloadBody, payload.body, payload.size);
}

int EaselControlClient::sendRequest(
        int handlerId,
        int rpcId,
        const ControlData &request) {
    if (request.size > EaselControlImpl::kMaxPayloadSize) {
        ALOGE("Request size out of boundary %" PRIu64, request.size);
        return -EINVAL;
    }

    EaselControlImpl::RpcMsg rpcMsg;
    getRpcMsg(handlerId, rpcId, false, request, &rpcMsg);

    EaselComm::EaselMessage msg;
    rpcMsg.getEaselMessage(&msg);

    int ret = easel_conn.sendMessage(&msg);
    if (ret) {
        ALOGE("%s: Failed to send request to Easel (%d)", __FUNCTION__, ret);
    }
    return ret;
}

int EaselControlClient::sendRequestWithCallback(
        int handlerId,
        int rpcId,
        const ControlData &request,
        std::function<void(const ControlData &response)> callback) {
    if (request.size > EaselControlImpl::kMaxPayloadSize) {
        ALOGE("Request size out of boundary %" PRIu64, request.size);
        return -EINVAL;
    }

    EaselControlImpl::RpcMsg rpcMsg;
    getRpcMsg(handlerId, rpcId, true, request, &rpcMsg);

    {
        std::lock_guard<std::mutex> lock(gCallbackMapMutex);
        ALOG_ASSERT(gCallbackMap.count(rpcMsg.callbackId) == 0);
        gCallbackMap[rpcMsg.callbackId] = callback;
    }

    EaselComm::EaselMessage msg;
    rpcMsg.getEaselMessage(&msg);

    int ret = easel_conn.sendMessage(&msg);
    if (ret) {
        {
            std::lock_guard<std::mutex> lock(gCallbackMapMutex);
            gCallbackMap.erase(rpcMsg.callbackId);
        }
        ALOGE("%s: Failed to send request to Easel (%d)\n", __FUNCTION__, ret);
    }
    return ret;
}

int EaselControlClient::startMipi(enum EaselControlClient::Camera camera, int rate)
{
    struct EaselStateManager::EaselMipiConfig config = {
        .rxRate = rate, .txRate = rate,
    };
    int ret;

    ALOGD("%s: camera %d, rate %d\n", __FUNCTION__, camera, rate);

    // TODO (b/36537557): intercept here to add functional mode
    config.mode = EaselStateManager::EaselMipiConfig::ESL_MIPI_MODE_BYPASS;

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
    int ret;

    ALOGD("%s\n", __FUNCTION__);

    ret = thermalMonitor.start();
    if (ret) {
        ALOGE("failed to start EaselThermalMonitor (%d)\n", ret);
        return ret;
    }

    ret = switchState(ControlState::RESUMED);
    if (ret) {
        ALOGE("Failed to resume Easel (%d)\n", ret);
        return ret;
    }

    // Starts logging client.
    ret = gLogClient.start();
    if (ret) {
        ALOGE("Failed to start LogClient (%d)\n", ret);
        return ret;
    }

    return 0;
}

// Called when the camera app is closed
int EaselControlClient::suspend() {
    int ret = 0;

    ALOGD("%s\n", __FUNCTION__);

    // Stops logging client before suspend.
    gLogClient.stop();

    ret = switchState(ControlState::SUSPENDED);
    if (ret) {
        ALOGE("%s: failed to suspend Easel (%d)\n", __FUNCTION__, ret);
    }

    ret = thermalMonitor.stop();
    if (ret) {
        ALOGE("%s: failed to stop EaselThermalMonitor (%d)\n", __FUNCTION__, ret);
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

    gCallbackMap.clear();

    ret = stateMgr.open();
    if (ret) {
        ALOGE("failed to initialize EaselStateManager (%d)\n", ret);
        return ret;
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
