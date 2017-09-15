#define LOG_TAG "EaselClockControl"

#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <log/log.h>

#include "easelcontrol.h"
#include "EaselClockControl.h"

#define LPDDR_SYS_FILE                "/sys/kernel/mnh_freq_cool/lpddr_freq"
#define CPU_SYS_FILE                  "/sys/kernel/mnh_freq_cool/cpu_freq"
#define IPU_SYS_FILE                  "/sys/kernel/mnh_freq_cool/ipu_freq"
#define IPU_CLK_SRC_SYS_FILE          "/sys/kernel/mnh_freq_cool/ipu_clk_src"
#define SYS200_SYS_FILE               "/sys/kernel/mnh_freq_cool/sys200"
#define LPDDR_SYS200_SYS_FILE         "/sys/kernel/mnh_freq_cool/lpddr_sys200"
#define IPU_CLOCK_GATING_SYS_FILE     "/sys/kernel/mnh_freq_cool/ipu_clock_gating"
#define PCIE_POWER_MODE_FILE          "/sys/devices/platform/200000.pcie/power_mode"

#define PCIE_POWER_MODE_CLKPM_ENABLE    (1 << 0)
#define PCIE_POWER_MODE_L1_2_ENABLE     (1 << 1)
#define PCIE_POWER_MODE_AXI_CG_ENABLE   (1 << 2)

#define LPDDR_MIN_FREQ 33

static const int fspIndexToFrequency[] = {33, 400, 1600, 2400};
static const int validCpuFrequencies[] = {200, 400, 600, 800, 950};
static const int validIpuFrequencies[] = {100, 200, 300, 400, 425};

static enum EaselClockControl::Mode mMode = (enum EaselClockControl::Mode)-EINVAL;
static enum EaselThermalMonitor::Condition mThermalCondition =
    (enum EaselThermalMonitor::Condition)-EINVAL;

struct ModeConfig {
    int lpddrFreq;
    int cpuFreq;
    int ipuFreq;
};

// The frequency configurations for Functional mode based on thermal conditions
static const std::map<enum EaselThermalMonitor::Condition, struct ModeConfig> functionalModeConfigs = {
    { EaselThermalMonitor::Condition::Low,      {2400, 950, 425}},
    { EaselThermalMonitor::Condition::Medium,   {1600, 800, 425}},
    { EaselThermalMonitor::Condition::High,     {1600, 800, 300}},
    { EaselThermalMonitor::Condition::Critical, {1600, 800, 200}},
    { EaselThermalMonitor::Condition::Unknown,  {1600, 800, 200}},
};

int EaselClockControl::setMode(enum Mode mode, enum EaselThermalMonitor::Condition thermalCond)
{
    if ((mode == mMode) && (mThermalCondition == thermalCond)) {
        return 0;
    }

    switch (mode) {
        case Mode::Bypass:
        {
            ALOGI("%s: Bypass Mode (33/200/100)", __FUNCTION__);
            setIpuClockGating(true);
            setAxiClockGating(true);
            EaselClockControl::setSys200Mode();
            break;
        }

        case Mode::Capture:
        {
            ALOGI("%s: Capture Mode (400/200/200)", __FUNCTION__);
            setIpuClockGating(false);
            setAxiClockGating(false);
            setFrequency(Subsystem::LPDDR, 400);
            setFrequency(Subsystem::CPU, 200);
            setFrequency(Subsystem::IPU, 200);
            break;
        }

        case Mode::Functional:
        {
            struct ModeConfig cfg = functionalModeConfigs.at(thermalCond);

            ALOGI("%s: Functional Mode (%d/%d/%d)", __FUNCTION__, cfg.lpddrFreq, cfg.cpuFreq,
                  cfg.ipuFreq);
            setIpuClockGating(false);
            setAxiClockGating(false);
            setFrequency(Subsystem::LPDDR, cfg.lpddrFreq);
            setFrequency(Subsystem::CPU, cfg.cpuFreq);
            setFrequency(Subsystem::IPU, cfg.ipuFreq);
            break;
        }

        default:
        {
            ALOGE("%s: Invalid operating mode %d", __FUNCTION__, mode);
            return -EINVAL;
        }
    }

    mMode = mode;
    mThermalCondition = thermalCond;

    return 0;
}

enum EaselClockControl::Mode EaselClockControl::getMode()
{
    return mMode;
}

bool EaselClockControl::isNewThermalCondition(enum EaselThermalMonitor::Condition thermalCond)
{
    return mThermalCondition != thermalCond;
}

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
            ALOGE("Invalid subsystem %d", system);
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
            ALOGE("Invalid subsystem %d", system);
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
        ALOGE("could not read lpddr frequency (%d)", ret);
        return ret;
    }

    ret = sscanf(buf, "FSP%d", &fspIndex);
    if (ret != 1) {
        ALOGE("bad format for lpddr frequency");
        return -EINVAL;
    }

    if (fspIndex >= ARRAY_SIZE(fspIndexToFrequency)) {
        ALOGE("invalid lpddr frequency");
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

int EaselClockControl::setSys200Mode()
{
    char buf[32] = "1";
    int ret;

    ret = writeSysFile((char*)SYS200_SYS_FILE, buf, 32);
    if (ret) {
        return ret;
    }

    return setLpddrFrequency(LPDDR_MIN_FREQ);
}

int EaselClockControl::setIpuClockGating(bool enable)
{
    char buf[32];

    ALOGI("%s: %d", __FUNCTION__, enable);

    snprintf(buf, 32, "%d", enable);
    return writeSysFile((char*)IPU_CLOCK_GATING_SYS_FILE, buf, 32);
}

int EaselClockControl::setAxiClockGating(bool enable)
{
    char buf[32];
    unsigned int val = 0;

    ALOGI("%s: %d", __FUNCTION__, enable);

    if (enable) {
        val = PCIE_POWER_MODE_CLKPM_ENABLE | PCIE_POWER_MODE_AXI_CG_ENABLE;
    } else {
        val = PCIE_POWER_MODE_CLKPM_ENABLE;
    }

    snprintf(buf, 32, "%d", val);

    return writeSysFile((char*)PCIE_POWER_MODE_FILE, buf, 32);
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
