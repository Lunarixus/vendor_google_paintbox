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

enum Mode { BYPASS, HDRPLUS } gMode;

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

    if (gMode != HDRPLUS)
        return 0;

    ALOGI("%s\n", __FUNCTION__);

    ret = stateMgr.waitForState(EaselStateManager::ESM_STATE_ACTIVE);
    if (ret == -EINVAL)
        ret = stateMgr.setState(EaselStateManager::ESM_STATE_ACTIVE);
    if (ret) {
        ALOGE("Failed to observe ACTIVE state (%d)\n", ret);
        return ret;
    }

    // Open easelcomm connection
    ret = easel_conn.open(EaselComm::EASEL_SERVICE_SYSCTRL);
    if (ret) {
        ALOGE("%s: Failed to open easelcomm connection", __FUNCTION__);
        return ret;
    }

    // start thread for handling easelcomm messages
    msg_handler_thread = new std::thread(msgHandlerThread);

    // Send a message with the new boottime base and time of day clock
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

    if (gMode != HDRPLUS)
        return 0;

    ALOGI("%s\n", __FUNCTION__);

    ctrl_msg.h.command = EaselControlImpl::CMD_DEACTIVATE;

    EaselComm::EaselMessage msg;
    msg.message_buf = &ctrl_msg;
    msg.message_buf_size = sizeof(ctrl_msg);
    msg.dma_buf = 0;
    msg.dma_buf_size = 0;
    ret = easel_conn.sendMessage(&msg);
    ALOG_ASSERT(ret == 0);

    // disable easelcomm polling thread
    msg_handler_thread->detach();
    delete msg_handler_thread;

    easel_conn.close();

    ret = stateMgr.setState(EaselStateManager::ESM_STATE_OFF);
    if (ret) {
        ALOGE("Could not deactivate Easel (%d)\n", ret);
        return ret;
    }

    return ret;
}

int EaselControlClient::startMipi(enum EaselControlClient::Camera camera, int rate)
{
    struct EaselStateManager::EaselMipiConfig config = {
        .rxRate = rate, .txRate = rate,
    };

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
    enum EaselStateManager::State state;
    int ret;

    ALOGI("%s\n", __FUNCTION__);

    ret = stateMgr.getState(&state);
    if (ret) {
        ALOGE("Could not read the current state of Easel (%d)\n", ret);
        return ret;
    }

    if ((state == EaselStateManager::ESM_STATE_PENDING) ||
        (state == EaselStateManager::ESM_STATE_ACTIVE)) {
        ALOGI("Easel is already powered, no need to resume it\n");
        return 0;
    }

    ret = stateMgr.setState(EaselStateManager::ESM_STATE_PENDING);
    if (ret) {
        ALOGE("Failed to set Easel to PENDING state (%d)\n", ret);
        return ret;
    }

    if (gMode == HDRPLUS) {
        ret = stateMgr.setState(EaselStateManager::ESM_STATE_ACTIVE, false /* blocking */);
        if (ret) {
            ALOGE("Failed to set Easel to ACTIVE state (%d)\n", ret);
            return ret;
        }
    }

    return 0;
}

// Called when the camera app is closed
int EaselControlClient::suspend() {
    int ret;

    ALOGI("%s\n", __FUNCTION__);

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

#ifdef ANDROID
    gMode = (enum Mode)property_get_int32("persist.camera.hdrplus.enable", 0);
#endif

    ret = stateMgr.open();
    if (ret) {
        ALOGE("failed to initialize EaselStateManager (%d)\n", ret);
        return ret;
    }

    if (ret) {
        ALOGE("%s: failed (%d)", __FUNCTION__, ret);
        stateMgr.close();
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

