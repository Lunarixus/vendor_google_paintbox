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

int EaselStateManager::open()
{
    mFd = ::open(ESM_DEV_FILE, O_RDONLY);

    if (mFd < 0)
        return -errno;

    return 0;
}

int EaselStateManager::close()
{
    if (mFd >= 0)
        return ::close(mFd);

    return 0;
}

int EaselStateManager::startMipi(struct EaselMipiConfig *config)
{
    struct mnh_mipi_config mnhConfig = {
        .txdev = config->txChannel,
        .rxdev = config->rxChannel,
        .rx_rate = config->rxRate,
        .tx_rate = config->txRate,
        .mode = config->mode,
        .vc_en_mask = MNH_MIPI_VC_ALL_EN_MASK,
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
