#define LOG_TAG "EaselStateManagerBlue"

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

#include "EaselStateManagerBlue.h"

#define ESM_DEV_FILE  "/dev/mnh_sm"

namespace android {
namespace EaselPowerBlue {

int EaselStateManager::open() {
  mFd = ::open(ESM_DEV_FILE, O_RDONLY);

  if (mFd < 0) return -errno;

  return 0;
}

int EaselStateManager::close() {
  int ret = 0;

  if (mFd >= 0) {
    ret = ::close(mFd);
    mFd = -1;
  }

  return ret;
}

int EaselStateManager::getState(enum EaselStateManager::State *state) {
  if (ioctl(mFd, MNH_SM_IOC_GET_STATE, reinterpret_cast<int*>(state)) == -1) {
    return -errno;
  }

  return 0;
}

int EaselStateManager::setState(enum EaselStateManager::State state,
                                bool blocking) {
  if (ioctl(mFd, MNH_SM_IOC_SET_STATE, static_cast<int>(state)) == -1) {
    return -errno;
  }

  if (blocking) {
    int res = waitForState(state);
    return res;
  }

  return 0;
}

int EaselStateManager::waitForPower() {
  if (ioctl(mFd, MNH_SM_IOC_WAIT_FOR_POWER) == -1)
    return -errno;

  return 0;
}

int EaselStateManager::waitForState(enum EaselStateManager::State state) {
  if (ioctl(mFd, MNH_SM_IOC_WAIT_FOR_STATE, static_cast<int>(state)) == -1) {
    return -errno;
  }

  return 0;
}

int EaselStateManager::getFwVersion(char *fwVersion, size_t size) {
  if ((fwVersion == nullptr) || (size < FW_VER_SIZE)) {
    return -EINVAL;
  }

  if (ioctl(mFd, MNH_SM_IOC_GET_FW_VER, fwVersion) == -1) {
    return -errno;
  }

  return 0;
}

} // namespace EaselPowerBlue
} // namespace android
