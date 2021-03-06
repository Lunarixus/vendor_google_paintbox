#define LOG_TAG "EaselStateManager"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <cutils/properties.h>
#include <log/log.h>

#include "uapi/linux/mnh-sm.h"

#include "EaselStateManager.h"

#define ESM_DEV_FILE  "/dev/mnh_sm"

int EaselStateManager::open()
{
    mFd = ::open(ESM_DEV_FILE, O_RDONLY);

    if (mFd < 0)
        return -errno;

    return 0;
}

int EaselStateManager::close()
{
    int ret = 0;

    if (mFd >= 0) {
        ret = ::close(mFd);
        mFd = -1;
    }

    return ret;
}

int EaselStateManager::startMipi(struct EaselMipiConfig *config)
{
    struct mnh_mipi_config mnhConfig = {
        .txdev = config->txChannel,
        .rxdev = config->rxChannel,
        .rx_rate = config->rxRate,
        .tx_rate = config->txRate,
        .mode = config->mode,
        .vc_en_mask = 0,
    };

    if (ioctl(mFd, MNH_SM_IOC_CONFIG_MIPI, &mnhConfig) == -1)
        return -errno;

    return 0;
}

int EaselStateManager::stopMipi(struct EaselMipiConfig *config)
{
    struct mnh_mipi_config mnhConfig = {
        .txdev = config->txChannel,
        .rxdev = config->rxChannel,
    };

    if (ioctl(mFd, MNH_SM_IOC_STOP_MIPI, &mnhConfig) == -1)
        return -errno;

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

    if (blocking) {
        int res = waitForState(state);
        return res;
    }

    return 0;
}

int EaselStateManager::waitForPower()
{
    if (ioctl(mFd, MNH_SM_IOC_WAIT_FOR_POWER) == -1)
        return -errno;

    return 0;
}

int EaselStateManager::waitForState(enum State state)
{
    if (ioctl(mFd, MNH_SM_IOC_WAIT_FOR_STATE, (int)state) == -1)
        return -errno;

    return 0;
}

int EaselStateManager::getFwVersion(char *fwVersion)
{
    if (fwVersion == NULL)
        return -1;

    if (ioctl(mFd, MNH_SM_IOC_GET_FW_VER, &fwVersion[0]) == -1)
        return -errno;

    return 0;
}
