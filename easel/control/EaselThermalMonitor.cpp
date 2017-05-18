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

// Lock used for synchronizing the monitor thread with the application thread.
std::mutex monitorMutex;

// Condition used to notify monitoring thread.
std::condition_variable monitorCondition;

// Flag for coordinating with the monitorThread
enum {
    MONITOR_STOP, // stop thread and return
    MONITOR_OFF,  // do not report status
    MONITOR_ON,   // report status
} monitorFlag;

// Thread for monitoring thermal zones
void monitor(EaselThermalMonitor *inst)
{
    if (inst == NULL) {
#ifdef EASELSERVER
        LOGE("%s: monitor thread received null pointers", __FUNCTION__);
#else
        ALOGE("%s: monitor thread received null pointers", __FUNCTION__);
#endif
        return;
    }

    std::unique_lock<std::mutex> monitor_lock(monitorMutex);
    while (monitorFlag != MONITOR_STOP) {
        if (monitorFlag == MONITOR_ON) {
            // Wait POLLING_INTERVAL seconds for any change in status; if it
            // times out, then print thermal status
            monitorCondition.wait_for(monitor_lock, std::chrono::seconds(POLLING_INTERVAL));
        } else {
            monitorCondition.wait(monitor_lock);
        }

        if (monitorFlag == MONITOR_ON) {
            inst->printStatus();
        }
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

    monitorFlag = MONITOR_OFF;
    mThread = new std::thread(monitor, this);

    return 0;
}

int EaselThermalMonitor::close()
{
    if (mThread) {
        {
            std::unique_lock<std::mutex> monitor_lock(monitorMutex);
            monitorFlag = MONITOR_STOP;
        }
        monitorCondition.notify_all();
        mThread->join();
        delete mThread;
        mThread = NULL;
    }

    for (auto zone : mZones) {
        delete zone;
    }
    mZones.clear();

    return 0;
}

int EaselThermalMonitor::start()
{
    {
        std::unique_lock<std::mutex> monitor_lock(monitorMutex);
        monitorFlag = MONITOR_ON;
    }
    monitorCondition.notify_all();

    return 0;
}

int EaselThermalMonitor::stop()
{
    {
        std::unique_lock<std::mutex> monitor_lock(monitorMutex);
        monitorFlag = MONITOR_OFF;
    }
    monitorCondition.notify_all();

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
