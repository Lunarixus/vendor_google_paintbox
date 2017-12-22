#ifndef __THERMAL_ZONE_H__
#define __THERMAL_ZONE_H__

#include <stdio.h>
#include <string>

/*
 * Abstracts the thermal zone sysfs framework for reading thermal sensors
 */

class ThermalZone {
public:
    static const int kMaxCharBufferLen = 100;

    ThermalZone(const std::string &name, int scaling);

    /*
     * Opens the thermal zone sysfs file.
     *
     * Returns 0 on success of -errno on failure.
     */
    int open();

    /*
     * Closes thermal zone sysfs file.
     *
     * Returns 0 on success of -errno on failure.
     */
    int close();

    /*
     * Returns the name of the zone.
     */
    std::string getName();

    /*
     * Returns the current temperature in millidegree celsius.
     */
    int getTemp();

private:
    /*
     * The name of the thermal zone. Should match the name in the "type" file in
     * the sysfs framework.
     */
    std::string mName;

    /*
     * The file descriptor to the "temp" file in the sysfs framework.
     */
    int mFd;

    /*
     * A scaling factor to the temperature to convert the reading of the "temp"
     * file into millidegrees celsius.
     */
    int mScaling;

    /*
     * A local function for finding the thermal zone in the sysfs framework.
     */
    int findFile(const std::string &name);
};

#endif // __THERMAL_ZONE_H__
