#ifndef __EASEL_THERMAL_MONITOR_H__
#define __EASEL_THERMAL_MONITOR_H__

#include <atomic>
#include <stdio.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "ThermalZone.h"

/*
 * This class periodically monitors the temperature of kernel thermal zones
 */
class EaselThermalMonitor {
public:
    struct Configuration {
        /*
         * The name of the thermal zone; should be the same as the "type" field
         * in the sysfs thermal framework.
         */
        std::string name;

        /*
         * A scaling factor applied to the temperature reading of each thermal
         * zone. This is useful when systems do not correctly report temperature
         * in the right units.
         */
        int scaling;
    };

    /*
     * Initializes the instance with the names of the thermal zones to monitor.
     *
     * Returns 0 on success or -errno for failure.
     */
    int open(const std::vector<struct EaselThermalMonitor::Configuration> &cfg);

    /*
     * Closes the instance and all files used in monitoring.
     *
     * Returns 0 on success or -errno for failure.
     */
    int close();

    /*
     * Starts monitoring device temperatures.
     *
     * Returns 0 for success; otherwise, returns error number.
     */
    int start();

    /*
     * Stops monitoring device temperatures.
     *
     * config: describes MIPI configuration options.
     *
     * Returns 0 for success; otherwise, returns error number.
     */
    int stop();

    /*
     * Prints current temperature of monitored thermal zones
     */
    void printStatus();

private:
    /*
     * A vector of thermal zones, allocated during open()
     */
    std::vector<ThermalZone*> mZones;

    /*
     * Thread for monitoring thermal zones
     */
    std::thread *mThread;

    /*
     * Atomic condition flag for stopping monitorThread
     */
    std::atomic_bool mThreadStopFlag;
};

#endif // __EASEL_THERMAL_MONITOR_H__
