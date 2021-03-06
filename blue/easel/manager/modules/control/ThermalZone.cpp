#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <string>

#include "ThermalZone.h"

#define THERMAL_ZONE_SYSFS_PATH  "/sys/class/thermal"

ThermalZone::ThermalZone(const std::string &name, int scaling) {
  mName = name;
  mScaling = scaling;
}

int ThermalZone::open() {
  mFd = findFile(mName);
  if (mFd < 0)
    return mFd;

  return 0;
}

int ThermalZone::close() {
  int ret = 0;

  if (mFd >= 0) {
    ret = ::close(mFd);
    mFd = -1;
  }

  return ret;
}

std::string ThermalZone::getName() {
  return mName;
}

int ThermalZone::getTemp() {
  char buffer[32];

  if (mFd < 0)
    return -1;

  lseek(mFd, 0, SEEK_SET);
  if (read(mFd, buffer, 32) < 0)
    return 0;
  return atoi(buffer) * mScaling;
}

int ThermalZone::findFile(const std::string &name) {
  DIR *thermal_dir = opendir(THERMAL_ZONE_SYSFS_PATH);
  struct dirent *entry;
  int retFd = -ENOENT;

  if (thermal_dir == NULL)
    return -errno;

  while ((entry = readdir(thermal_dir)) != NULL) {
    if (strncmp(entry->d_name, "thermal_zone", 12) == 0) {
      char filename[kMaxCharBufferLen];
      int fd;

      // check to see if this is the intended thermal zone entry
      snprintf(filename, kMaxCharBufferLen, "%s/%s/type",
               THERMAL_ZONE_SYSFS_PATH, entry->d_name);
      fd = ::open(filename, O_RDONLY);
      if (fd >= 0) {
        char buffer[kMaxCharBufferLen];

        read(fd, buffer, kMaxCharBufferLen);
        ::close(fd);
        if (strncmp(buffer, name.c_str(), strlen(name.c_str())) == 0) {
          snprintf(filename, kMaxCharBufferLen, "%s/%s/temp",
                   THERMAL_ZONE_SYSFS_PATH, entry->d_name);
          retFd = ::open(filename, O_RDONLY);
          break;
        }
      }
    }
  }

  closedir(thermal_dir);

  return retFd;
}
