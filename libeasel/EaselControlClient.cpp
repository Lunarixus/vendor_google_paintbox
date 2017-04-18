#define LOG_TAG "EaselControlClient"

#include "EaselStateManager.h"
#include "easelcontrol.h"
#include "easelcontrol_impl.h"
#include "mockeaselcomm.h"

#include <thread>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unordered_map>

#ifdef ANDROID
#include <android/log.h>
#include <cutils/properties.h>
#include <utils/Log.h>
#endif

#define ESM_DEV_FILE    "/dev/mnh_sm"
#define NSEC_PER_SEC    1000000000ULL

namespace {
#ifdef MOCKEASEL
// Mock EaselControl uses mock EaselComm
EaselCommClientNet easel_conn;
#else
EaselCommClient easel_conn;
#endif

// EaselStateManager instances
EaselStateManager stateMgr;

// Incoming message handler thread
std::thread *msg_handler_thread;

// Lock to guard gCallbackMap
std::mutex gCallbackMapMutex;
// Map from callbackId to callback functions.
std::unordered_map<int, std::function<void(const ControlData &)>> gCallbackMap;

// Mutex and condition to synchronize easel_conn
std::mutex easel_conn_mutex;
std::condition_variable easel_conn_cond;
bool easel_conn_ready;

/*
 * Handle CMD_LOG Android logging control message received from server.
 */
void handleLog(const EaselControlImpl::LogMsg *msg) {
    char *tag = (char *)msg + sizeof(EaselControlImpl::LogMsg);
    char *text = tag + msg->tag_len;
#ifdef ANDROID
    __android_log_write(msg->prio, tag, text);
#else
    printf("<%d> %s %s\n", msg->prio, tag, text);
#endif
}


/*
 * Handle incoming messages from EaselControlServer.
 */
void msgHandlerThread() {
    int ret;

    // Grab the lock while we are trying to initialize easel_conn
    std::unique_lock<std::mutex> lk(easel_conn_mutex);

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

    // Notify the other thread that easel_conn is ready to send/receive messages
    easel_conn_ready = true;
    lk.unlock();
    easel_conn_cond.notify_one();

#ifdef ANDROID
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
#endif

    while (true) {
        EaselComm::EaselMessage msg;
        int ret = easel_conn.receiveMessage(&msg);
        if (ret) {
            if (errno != ESHUTDOWN)
#ifdef ANDROID
                ALOGI("easelcontrol: receiveMessage error (%d, %d), exiting\n", ret, errno);
#else
                fprintf(stderr,
                        "easelcontrol: receiveMessage error (%d, %d), exiting\n", ret, errno);
#endif
            break;
        }

        if (msg.message_buf == nullptr)
            continue;

        EaselControlImpl::MsgHeader *h =
            (EaselControlImpl::MsgHeader *)msg.message_buf;

        switch (h->command) {
        case EaselControlImpl::CMD_LOG:
            handleLog((EaselControlImpl::LogMsg *)msg.message_buf);
            break;
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

int EaselControlClient::activate() {
    int ret;

    ALOGI("%s\n", __FUNCTION__);

    ret = stateMgr.waitForState(EaselStateManager::ESM_STATE_ACTIVE);
    if (ret == -EINVAL)
        ret = stateMgr.setState(EaselStateManager::ESM_STATE_ACTIVE);
    if (ret) {
        ALOGE("Failed to observe ACTIVE state (%d)\n", ret);
        return ret;
    }

    // Wait until easel_conn is ready
    std::unique_lock<std::mutex> lk(easel_conn_mutex);
    easel_conn_cond.wait(lk, []{return easel_conn_ready;});

    // Send an active message with the new boottime base and time of day clock
    EaselControlImpl::ActivateMsg ctrl_msg;
    ctrl_msg.h.command = EaselControlImpl::CMD_ACTIVATE;
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

    ret = easel_conn.sendMessage(&msg);
    if (ret) {
        ALOGE("%s: Failed to send message to Easel (%d)\n", __FUNCTION__, ret);
        return ret;
    }

    return 0;
}

int EaselControlClient::deactivate() {
    EaselControlImpl::DeactivateMsg ctrl_msg;
    int ret;

    ALOGI("%s\n", __FUNCTION__);

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

    ALOGI("%s: camera %d, rate %d\n", __FUNCTION__, camera, rate);

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

    ALOGI("%s: camera %d\n", __FUNCTION__, camera);

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

    ALOGI("%s\n", __FUNCTION__);

    ret = stateMgr.setState(EaselStateManager::ESM_STATE_ACTIVE, false /* blocking */);
    if (ret) {
        ALOGE("Failed to resume Easel (%d)\n", ret);
        return ret;
    }

    // start thread for opening easel_conn channel and handling easelcomm messages
    msg_handler_thread = new std::thread(msgHandlerThread);

    return 0;
}

// Called when the camera app is closed
int EaselControlClient::suspend() {
    EaselControlImpl::DeactivateMsg ctrl_msg;
    int ret;

    ALOGI("%s\n", __FUNCTION__);

    ctrl_msg.h.command = EaselControlImpl::CMD_SUSPEND;

    if (easel_conn_ready) {
        EaselComm::EaselMessage msg;
        msg.message_buf = &ctrl_msg;
        msg.message_buf_size = sizeof(ctrl_msg);
        msg.dma_buf = 0;
        msg.dma_buf_size = 0;
        ret = easel_conn.sendMessage(&msg);
        if (ret) {
            ALOGE("%s: failed to send suspend command to Easel (%d)\n", __FUNCTION__, ret);
        }

        // disable easelcomm polling thread
        if (msg_handler_thread) {
            msg_handler_thread->detach();
            delete msg_handler_thread;
            msg_handler_thread = NULL;
        }

        easel_conn.close();
    }

    ret = stateMgr.setState(EaselStateManager::ESM_STATE_OFF);
    if (ret) {
        ALOGE("%s: failed to suspend Easel (%d)\n", __FUNCTION__, ret);
        return ret;
    }

    return ret;
}

int EaselControlClient::open() {
    int ret = 0;

    ALOGI("%s\n", __FUNCTION__);

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
    stateMgr.setState(EaselStateManager::ESM_STATE_OFF);
    stateMgr.close();
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
