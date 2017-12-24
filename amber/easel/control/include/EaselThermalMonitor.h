#ifndef __EASEL_THERMAL_MONITOR_H__
#define __EASEL_THERMAL_MONITOR_H__

#include <atomic>
#include <stdio.h>
#include <string>
#include <thread>
#include <tuple>
#include <unistd.h>
#include <vector>

#include "ThermalZone.h"

/*
 * This class periodically monitors the temperature of kernel thermal zones
 */
class EaselThermalMonitor {
public:
    enum Condition {
        /* Recognized thermal conditions, ranges are defined when calling
         * start function below.
         */
        Low,
        Medium,
        High,
        Critical,
        /* Special case thermal condition if no thermal zones are valid */
        Unknown,
    };

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

        /* An array of temperature thresholds in millidegree celsius
         * that are used when calculating the thermal condition:
         *     Low:       0 <= temperature < thresholds[Low]
         *     Medium:    thresholds[Low] <= temperature < thresholds[Medium]
         *     High:      thresholds[Medium] <= temperature < thresholds[High]
         *     Critical:  thresholds[High] <= temperature
         */
        int thresholds[3];
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
     * Get the current thermal condition.
     *
     * Returns the current thermal condition as calculated during the last
     * monitor event.
     */
    enum EaselThermalMonitor::Condition getCondition();

    /*
     * Checks the thermal monitors for the current condition.
     *
     * Returns the highest Condition of all thermal zones at the point this
     * function is called.
     */
    enum EaselThermalMonitor::Condition checkCondition();

private:
    using ThermalZoneConfig = std::tuple<ThermalZone*, int*>;

    /*
     * A vector of thermal zone configurations, allocated during open()
     */
    std::vector<ThermalZoneConfig> mZoneCfgs;

    /*
     * Thread for monitoring thermal zones
     */
    std::thread *mThread;

    /*
     * The current thermal condition
     */
    enum Condition mCondition;
};

#endif // __EASEL_THERMAL_MONITOR_H__
