#define LOG_TAG "EaselThermalMonitor"

#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <iomanip>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <thread>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#ifdef EASELSERVER
#include "easelcontrol.h"
#endif

#include <log/log.h>

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
        ALOGE("%s: monitor thread received null pointers", __FUNCTION__);
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
            inst->checkCondition();
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
            ALOGE("Could not open zone \"%s\" (%d)", cfg[i].name.c_str(), ret);
            continue;
        }

        mZoneCfgs.push_back(std::make_tuple(zone, (int*)cfg[i].thresholds));
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

    for (auto cfg : mZoneCfgs) {
        ThermalZone *zone = std::get<0>(cfg);
        zone->close();
        delete zone;
    }
    mZoneCfgs.clear();

    return 0;
}

int EaselThermalMonitor::start()
{
    {
        std::unique_lock<std::mutex> monitor_lock(monitorMutex);
        monitorFlag = MONITOR_ON;
    }
    monitorCondition.notify_all();

    mCondition = Condition::Unknown;
    return 0;
}

int EaselThermalMonitor::stop()
{
    {
        std::unique_lock<std::mutex> monitor_lock(monitorMutex);
        monitorFlag = MONITOR_OFF;
    }
    monitorCondition.notify_all();

    mCondition = Condition::Unknown;
    return 0;
}

enum EaselThermalMonitor::Condition EaselThermalMonitor::getCondition()
{
    return mCondition;
}

enum EaselThermalMonitor::Condition EaselThermalMonitor::checkCondition()
{
    std::stringstream ss, ss2;
    enum Condition maxCondition = Condition::Unknown;

    for (auto cfg : mZoneCfgs) {
        ThermalZone *zone = std::get<0>(cfg);
        int *thresholds = std::get<1>(cfg);
        int temperature = zone->getTemp();
        enum Condition currentCondition = Condition::Unknown;

        if (temperature >= 0) {
            int i;
            for (i = Condition::Low; i < Condition::High; i++) {
                if (temperature < thresholds[i]) {
                    break;
                }
            }
            currentCondition = static_cast<enum Condition>(i);
        }

        if ((maxCondition == Condition::Unknown) || (currentCondition > maxCondition)) {
            maxCondition = currentCondition;
        }

        if (ss.tellp() > 0) {
            ss << ", ";
        }

        ss << zone->getName() << ": ";
        ss << std::fixed << std::setprecision(2) << temperature / 1000.0f;
    }

    switch (maxCondition) {
        case Condition::Low: ss2 << "(Low)"; break;
        case Condition::Medium: ss2 << "(Medium)"; break;
        case Condition::High: ss2 << "(High)"; break;
        case Condition::Critical: ss2 << "(Critical)"; break;
        default: ss2 << "(Unknown)"; break;
    }

    ALOGI("%s %s", ss2.str().c_str(), ss.str().c_str());

    mCondition = maxCondition;
    return maxCondition;
}

