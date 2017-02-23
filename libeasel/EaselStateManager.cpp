#define LOG_TAG "EaselStateManager"

#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <utils/Log.h>

#include "uapi/linux/mnh-sm.h"

#include "EaselStateManager.h"

#define ESM_DEV_FILE  "/dev/mnh_sm"

int EaselStateManager::init()
{
    mFd = open(ESM_DEV_FILE, O_RDONLY);
    if (mFd >= 0)
        return 0;
    else
        return -1;
}

int EaselStateManager::powerOn(bool blocking)
{
    if (ioctl(mFd, MNH_SM_IOC_POWERON, NULL) == -1)
        return -errno;

    if (blocking)
        return waitForState(ESM_STATE_INIT);

    return 0;
}

int EaselStateManager::powerOff(bool blocking)
{
    if (ioctl(mFd, MNH_SM_IOC_POWEROFF, NULL) == -1)
        return -errno;

    if (blocking)
        return waitForState(ESM_STATE_OFF);

    return 0;
}

int EaselStateManager::configMipi(struct EaselMipiConfig *config)
{
    struct mnh_mipi_config mnhConfig = {
        .txdev = config->txChannel,
        .rxdev = config->rxChannel,
        .rx_rate = config->rxRate,
        .tx_rate = config->txRate,
        .vc_en_mask = MNH_MIPI_VC_ALL_EN_MASK,
        .is_gen3 = 1,
    };

    if (ioctl(mFd, MNH_SM_IOC_CONFIG_MIPI, &mnhConfig) == -1)
        return -errno;

    return 0;
}

int EaselStateManager::configDdr(bool blocking)
{
    if (ioctl(mFd, MNH_SM_IOC_CONFIG_DDR, NULL) == -1)
        return -errno;

    if (blocking)
        return waitForState(ESM_STATE_CONFIG_DDR);

    return 0;
}

int EaselStateManager::download(bool blocking)
{
    if (ioctl(mFd, MNH_SM_IOC_DOWNLOAD, NULL) == -1)
        return -errno;

    if (blocking)
        return waitForState(ESM_STATE_ACTIVE);

    return 0;
}

int EaselStateManager::getState(enum EaselStateManager::State *state)
{
    if (ioctl(mFd, MNH_SM_IOC_GET_STATE, (int *)state) == -1)
        return -errno;

    return 0;
}

int EaselStateManager::setState(enum EaselStateManager::State state, bool blocking)
{
    if (ioctl(mFd, MNH_SM_IOC_SET_STATE, (int)state) == -1)
        return -errno;

    if (blocking)
        return waitForState(state);

    return 0;
}

#define LOOP_DELAY_US 100
int EaselStateManager::waitForState(enum State state)
{
    enum EaselStateManager::State currState;
    int ret;
    int loopCount;

    switch (state) {
        case ESM_STATE_OFF:
            loopCount = 50000 / LOOP_DELAY_US;
            break;
        case ESM_STATE_INIT:
            loopCount = 100000 / LOOP_DELAY_US;
            break;
        case ESM_STATE_CONFIG_DDR:
            loopCount = 20000 / LOOP_DELAY_US;
            break;
        case ESM_STATE_ACTIVE:
            loopCount = 1000000 / LOOP_DELAY_US;
            break;
        default:
            return -EINVAL;
    }

    for (int i = 0; i < loopCount; i++) {
        ret = getState(&currState);
        if (ret < 0)
            return ret;
        else if (currState == state)
            return 0;

        usleep(LOOP_DELAY_US);
    }

    return -ETIMEDOUT;
}
