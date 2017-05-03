#define LOG_TAG "EaselThermalMonitor"

#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#ifdef EASELSERVER
#include "easelcontrol.h"
#include "EaselLog.h"
#else
#include <utils/Log.h>
#endif

#include "EaselThermalMonitor.h"

#define POLLING_INTERVAL 5 // seconds

// Thread for monitoring thermal zones
void monitor(EaselThermalMonitor *inst, std::atomic_bool *stopFlag)
{
    if ((inst == NULL) || (stopFlag == NULL)) {
#ifdef EASELSERVER
        LOGE("%s: monitor thread received null pointers", __FUNCTION__);
#else
        ALOGE("%s: monitor thread received null pointers", __FUNCTION__);
#endif
        return;
    }

    while (!*stopFlag) {
        sleep(POLLING_INTERVAL);
        inst->printStatus();
    }
}

int EaselThermalMonitor::open(const std::vector<struct Configuration> &cfg)
{
    int count = cfg.size();
    int ret;

    for (int i = 0; i < count; i++) {
        ThermalZone *zone = new ThermalZone(cfg[i].name, cfg[i].scaling);
        if (zone == NULL) {
            continue;
        }

        ret = zone->open();
        if (ret) {
#ifdef EASELSERVER
            LOGE("Could not open zone \"%s\" (%d)", cfg[i].name.c_str(), ret);
#else
            ALOGE("Could not open zone \"%s\" (%d)", cfg[i].name.c_str(), ret);
#endif
            continue;
        }

        mZones.push_back(zone);
    }

    return 0;
}

int EaselThermalMonitor::close()
{
    for (auto zone : mZones) {
        delete zone;
    }
    mZones.clear();

    return 0;
}

int EaselThermalMonitor::start()
{
    mThreadStopFlag = 0;
    mThread = new std::thread(monitor, this, &mThreadStopFlag);

    return 0;
}

int EaselThermalMonitor::stop()
{
    if (mThread) {
        mThreadStopFlag = 1;
        mThread->detach();
        delete mThread;
        mThread = NULL;
    }

    return 0;
}

void EaselThermalMonitor::printStatus()
{
    char buf[128] = "";

    for (auto zone : mZones) {
        char buf2[128];

        if (strlen(buf)) {
            strcat(buf, ", ");
        }

        sprintf(buf2, "%s: %.2f", zone->getName().c_str(), zone->getTemp() / 1000.0f);
        strcat(buf, buf2);
    };

#ifdef EASELSERVER
    LOGI("%s", buf);
#else
    ALOGI("%s", buf);
#endif
}
