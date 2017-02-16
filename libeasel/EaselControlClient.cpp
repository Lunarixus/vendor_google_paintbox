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
#endif
#ifdef ANDROID
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

int EaselControlClient::activateEasel(int sleepTime) {
    int ret;

    ret = stateMgr.setState(EaselStateManager::ESM_STATE_ACTIVE, true /* blocking */);
    ALOG_ASSERT(ret == 0);

    /* give some time for Easel to boot */
    sleep(sleepTime);

    ret = easel_conn.open(EaselComm::EASEL_SERVICE_SYSCTRL);
    if (ret) {
        ALOGE("%s: %d", __FUNCTION__, __LINE__);
        return ret;
    }
    msg_handler_thread = new std::thread(msgHandlerThread);

    /*
     * Send a message with the new boottime base and time of day clock.
     */
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
    ALOGE("%s: %d", __FUNCTION__, __LINE__);
    return easel_conn.sendMessage(&msg);
}

int EaselControlClient::deactivateEasel() {
    EaselControlImpl::DeactivateMsg ctrl_msg;
    int ret;

    ctrl_msg.h.command = EaselControlImpl::CMD_DEACTIVATE;

    EaselComm::EaselMessage msg;
    msg.message_buf = &ctrl_msg;
    msg.message_buf_size = sizeof(ctrl_msg);
    msg.dma_buf = 0;
    msg.dma_buf_size = 0;
    ret = easel_conn.sendMessage(&msg);
    ALOG_ASSERT(ret == 0);

    easel_conn.close();

    ret = stateMgr.setState(EaselStateManager::ESM_STATE_INIT);
    ALOG_ASSERT(ret == 0);

    return ret;
}

int EaselControlClient::configMipi(enum EaselControlClient::Camera camera, int rate)
{
    struct EaselStateManager::EaselMipiConfig config = {
        .rxRate = rate, .txRate = rate,
    };

    ALOGI("configMipi: camera %d, rate %d\n", camera, rate);

    if (camera == EaselControlClient::MAIN) {
        config.rxChannel = EaselStateManager::EaselMipiConfig::ESL_MIPI_RX_CHAN_0;
        config.txChannel = EaselStateManager::EaselMipiConfig::ESL_MIPI_TX_CHAN_0;
    } else {
        config.rxChannel = EaselStateManager::EaselMipiConfig::ESL_MIPI_RX_CHAN_1;
        config.txChannel = EaselStateManager::EaselMipiConfig::ESL_MIPI_TX_CHAN_1;
    }

    return stateMgr.configMipi(&config);
}

// Called when the camera app is opened
int EaselControlClient::resumeEasel() {
    int ret;
    ret = stateMgr.setState(EaselStateManager::ESM_STATE_INIT);
    ALOG_ASSERT(ret == 0);
    return ret;
}

// Called when the camera app is closed
int EaselControlClient::suspendEasel() {
    int ret;
    ret = stateMgr.setState(EaselStateManager::ESM_STATE_OFF);
    ALOG_ASSERT(ret == 0);
    return ret;
}

int EaselControlClient::open() {
    int ret;

    ret = stateMgr.init();
    if (ret) {
        ALOGE("failed to initialize EaselStateManager (%d)\n", ret);
        return ret;
    }

    ret = stateMgr.setState(EaselStateManager::ESM_STATE_INIT);
    if (ret) {
        ALOGE("failed to power on EaselStateManager (%d)\n", ret);
        return ret;
    }

    return 0;
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

