#define LOG_TAG "EaselClockControl"

#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <utils/Log.h>
#include "easelcontrol.h"
#include "EaselClockControl.h"

#define LPDDR_SYS_FILE        "/sys/kernel/mnh_freq_cool/lpddr_freq"
#define CPU_SYS_FILE          "/sys/kernel/mnh_freq_cool/cpu_freq"
#define IPU_SYS_FILE          "/sys/kernel/mnh_freq_cool/ipu_freq"
#define IPU_CLK_SRC_SYS_FILE  "/sys/kernel/mnh_freq_cool/ipu_clk_src"
#define SYS200_SYS_FILE       "/sys/kernel/mnh_freq_cool/sys200"

#define LOGE(fmt, ...) do { \
    easelLog(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__); \
  } while(0);


#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

static const int fspIndexToFrequency[] = {132, 1200, 2400, 1600};
static const int validCpuFrequencies[] = {200, 400, 600, 800, 950};
static const int validIpuFrequencies[] = {100, 200, 300, 400, 425};

int EaselClockControl::getFrequency(enum Subsystem system)
{
    switch (system) {
        case Subsystem::CPU:
            return getProcessorFrequency((char*)CPU_SYS_FILE);
        case Subsystem::IPU:
            return getProcessorFrequency((char*)IPU_SYS_FILE);
        case Subsystem::LPDDR:
            return getLpddrFrequency();
        default:
            LOGE("Invalid subsystem %d\n", system);
            return -EINVAL;
    }
}

int EaselClockControl::setFrequency(enum Subsystem system, int freq)
{
    switch (system) {
        case Subsystem::CPU:
            return setProcessorFrequency((char*)CPU_SYS_FILE, validCpuFrequencies,
                                         ARRAY_SIZE(validCpuFrequencies), freq);
        case Subsystem::IPU:
            return setProcessorFrequency((char*)IPU_SYS_FILE, validIpuFrequencies,
                                         ARRAY_SIZE(validIpuFrequencies), freq);
        case Subsystem::LPDDR:
            return setLpddrFrequency(freq);
        default:
            LOGE("Invalid subsystem %d\n", system);
            return -EINVAL;
    }
}

int EaselClockControl::getLpddrFrequency()
{
    char buf[32];
    int ret;
    unsigned int fspIndex;

    ret = readSysFile((char*)LPDDR_SYS_FILE, buf, 32);
    if (ret) {
        LOGE("could not read lpddr frequency (%d)\n", ret);
        return ret;
    }

    ret = sscanf(buf, "FSP%d", &fspIndex);
    if (ret != 1) {
        LOGE("bad format for lpddr frequency\n");
        return -EINVAL;
    }

    if (fspIndex >= ARRAY_SIZE(fspIndexToFrequency)) {
        LOGE("invalid lpddr frequency\n");
        return -EINVAL;
    }

    return fspIndexToFrequency[fspIndex];
}

int EaselClockControl::getProcessorFrequency(char *sysFile)
{
    char buf[32];
    int ret;
    int freq;

    ret = readSysFile(sysFile, buf, 32);
    if (ret) {
        return ret;
    }

    ret = sscanf(buf, "%dMHz", &freq);
    if (ret != 1) {
        return -EINVAL;
    }

    return freq;
}

int EaselClockControl::setLpddrFrequency(int freq)
{
    unsigned int i;
    char buf[32];

    for (i = 0; i < ARRAY_SIZE(fspIndexToFrequency); i++) {
        if (freq == fspIndexToFrequency[i])
            break;
    }

    if (i == ARRAY_SIZE(fspIndexToFrequency)) {
        return -EINVAL;
    }

    snprintf(buf, 32, "%d", i);

    return writeSysFile((char*)LPDDR_SYS_FILE, buf, 32);
}

int EaselClockControl::setProcessorFrequency(char *sysFile, const int *validFrequencies,
                                             int validCnt, int freq)
{
    int i;
    char buf[32];

    for (i = 0; i < validCnt; i++) {
        if (freq > validFrequencies[i])
            continue;
        else
            break;
    }

    if (i == validCnt) {
        return -EINVAL;
    }

    snprintf(buf, 32, "%d", validFrequencies[i]);

    return writeSysFile(sysFile, buf, 32);
}

int EaselClockControl::getSys200Mode(bool *enable)
{
    char buf[32];
    int ret;
    int val;

    ret = readSysFile((char*)SYS200_SYS_FILE, buf, 32);
    if (ret) {
        return ret;
    }

    errno = 0;
    val = strtol(buf, NULL, 10);
    if (errno && (val == 0)) {
        return errno;
    }

    if ((val != 0) && (val != 1)) {
        return -EINVAL;
    }

    *enable = (val) ? true : false;
    return 0;
}

int EaselClockControl::setSys200Mode(bool enable)
{
    char buf[32];

    snprintf(buf, 32, "%d", enable);

    return writeSysFile((char*)SYS200_SYS_FILE, buf, 32);
}

int EaselClockControl::openSysFile(char *file)
{
    return ::open(file, O_RDWR);
}

int EaselClockControl::closeSysFile(int fd)
{
    if (fd >= 0) {
        return ::close(fd);
    }

    return 0;
}

int EaselClockControl::readSysFile(char *file, char *buf, size_t count)
{
    int fd;
    int ret = 0;

    fd = openSysFile(file);
    if (fd < 0) {
        return fd;
    }

    if (read(fd, buf, count) < 0) {
        ret = -errno;
        closeSysFile(fd);
        return ret;
    } else {
        buf[count-1] = '\0';
    }

    return closeSysFile(fd);
}

int EaselClockControl::writeSysFile(char *file, char *buf, size_t count)
{
    int fd;
    int ret = 0;

    fd = openSysFile(file);
    if (fd < 0) {
        return fd;
    }

    if (write(fd, buf, count) < 0) {
        ret = -errno;
        closeSysFile(fd);
        return ret;
    }

    return closeSysFile(fd);
}