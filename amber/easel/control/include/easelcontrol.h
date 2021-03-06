/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Easel systemcontrol service API used by the Android framework and
 * corresponding Easel-side servers.
 */

#ifndef ANDROID_EASELCONTROL_H
#define ANDROID_EASELCONTROL_H

#include <array>
#include <functional>
#include <stdint.h>

#include "easelcomm.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

enum class EaselErrorSeverity {
    FATAL,          // Fatal error, expect caller restart EaselControlClient
    NON_FATAL,      // Non-fatal, caller may continue
    SEVERITY_COUNT,
};

enum class EaselErrorReason {
    LINK_FAIL,          // Power-on or PCIe link fail
    BOOTSTRAP_FAIL,     // AP didn't receive bootstrap msi
    OPEN_SYSCTRL_FAIL,  // AP failed to open SYSCTRL service
    HANDSHAKE_FAIL,     // Handshake failed
    IPU_RESET_REQ,      // Easel requested AP to reset it
    WATCHDOG,           // AP didn't receive periodic heartbeat from Easel
    REASON_COUNT,
};

/*
 * Callback on error.
 * r:  reason for error, value is one of enum EaselErrorReason
 * s:  severity for error, value is one of enum EaselErrorSeverity
 *
 * Please return 0 if the fatal error has been handled.
 */
using easel_error_callback_t = std::function<int(enum EaselErrorReason r, enum EaselErrorSeverity s)>;

class EaselControlClient {
public:
    enum Camera { MAIN, FRONT };

    /*
     * Open an easelcontrol connection to Easel.  Initialize easelcomm
     * communications for the easelcontrol service.
     *
     * Returns zero for success or -1 for failure.
     */
    int open(EaselService service_id = EASEL_SERVICE_SYSCTRL);
    /*
     * Temporary for TCP/IP-based mock.  Connects to the default easelcontrol
     * port on the named host.
     *
     * Returns zero for success or -1 for failure (errno is set).
     */
    int open(const char *easelhost);
    /*
     * Close the easelcontrol connection.
     */
    void close();
    /*
     * Activate Easel to HDR+ mode.
     *
     * Returns zero for success or error code for failure.
     */
    static int activate();
    /*
     * Deactivate Easel when not in HDR+ mode.
     *
     * Returns zero for success or error code for failure.
     */
    static int deactivate();
    /*
     * Retrieve Easel firmware version.
     *
     * Returns zero for success or error code for failure.
     */
    static int getFwVersion(char *fwVersion);
    /*
     * Start and/or configure one of the MIPI TX channels. This function will
     * block until Easel is powered.
     *
     * Returns zero for success or error code for failure.
     */
    static int startMipi(enum Camera camera, int rate,
                         bool enableIpu = false);
    /*
     * Stop one of the MIPI RX and TX channels.
     *
     * Returns zero for success or error code for failure.
     */
    static int stopMipi(enum Camera camera);
    /*
     * Resume Easel, as when the camera app is started.  If Easel is suspended
     * then this will resume it. This function is non-blocking.
     *
     * Returns zero for success or error code for failure.
     */
    static int resume();
    /*
     * Suspend Easel, as when the camera app is closed.
     *
     * Returns zero for success or error code for failure.
     */
    static int suspend();

    /*
     * Register a callback on error.
     *
     * The registered callback will only be called when error happens
     * on threads that are separate from serialized functions such as resume(),
     * suspend(), activate(), startMipi(), etc.
     * User should continue to handle return value of resume(), suspend(), etc.
     */
    static void registerErrorCallback(easel_error_callback_t f);
};

class EaselControlServer {
public:
    /*
     * Clock operating modes.
     *
     * Should match the modes used in EaselClockControl.h.
     */
    enum ClockMode {
        /*
         * Bypass mode is our lowest-power operating mode. We clock and power
         * gate the IPU. We slow all internal clocks to their lowest operating
         * mode. The kernel will continue to run, but will be very
         * low-performance.
         */
        Bypass,
        /*
         * Capture mode is the expected operating mode when capturing MIPI
         * frames to DRAM. We disable IPU clock gating, and raise the internal
         * clocks to the minimum levels that can support the workload.
         */
        Capture,
        /*
         * Functional mode is our highest-performance operating mode. We disable
         * IPU clock gating, and we raise the internal clocks to their highest
         * frequency. This mode also consumes the most power. The duration of
         * Functional mode should be much less frequent compared to Bypass and
         * Capture mode. In the future, this mode may be broken into multiple
         * levels allowing for various levels of performance/power depending on
         * the thermal environment.
         */
        Functional,
        Max,
    };

    enum ThermalCondition {
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

    /*
     * Open an easelcontrol connection.  Initialize easelcomm communications
     * for the easelcontrol service.
     *
     * Returns zero for success or -1 for failure.
     */
    int open(EaselService service_id = EASEL_SERVICE_SYSCTRL);
    /*
     * Close the easelcontrol connection.
     */
    void close();
    /*
     * Return the Easel-side clock that maintains a time sync'ed with the
     * AP-side CLOCK_BOOTTIME.  This value is compatible with Android
     * systemTime(CLOCK_BOOTTIME) values of type nsecs_t read at the same
     * time on the AP side.
     *
     * clockval points to the returned clock value in nanoseconds.
     *
     * Returns zero for success or negative errno value for failure.
     * -EAGAIN means the clock has not been updated since boot or Easel last
     *     activated.
     */
    static int getApSynchronizedClockBoottime(int64_t *clockval);

    /*
     * Convert the local Easel-side clock to the clock sync'ed with the
     * AP-side CLOCK_BOOTTIME.  This value is compatible with Android
     * systemTime(CLOCK_BOOTTIME) values of type nsecs_t read at the same
     * time on the AP side.
     *
     * localClockval is the input easel side clock value in nanoseconds.
     * apSyncedClockval is the converted ap synced clock value in nanoseconds.
     *
     * Returns zero for success or negative errno value for failure.
     * -EAGAIN means the clock has not been updated since boot or Easel last
     * activated.
     */
    static int localToApSynchronizedClockBoottime(
            int64_t localClockval, int64_t *apSyncedClockval);

    /*
     * Return the last-recorded Vsync timestamp.  The timestamp is recorded
     * by Easel upon Vsync interrupt, using a clock synchronized with the
     * AP-side CLOCK_BOOTTIME.
     *
     * timestamp points to the returned timestamp value in nanoseconds.
     *
     * Returns zero for success or negative errno value for failure.
     * -EAGAIN means no timestamp has been recorded since boot or Easel last
     *     activated.
     */
    static int getLastEaselVsyncTimestamp(int64_t *timestamp);

    /*
     * Sets the clock operating mode.
     *
     * Returns the thermal condition that was used when calling
     * EaselClockControl::setMode.
     */
    static ThermalCondition setClockMode(ClockMode mode);

    /*
     * Returns the current clock operating mode.
     */
    static ClockMode getClockMode();

    /*
     * Returns the current thermal condition
     */
    static ThermalCondition getThermalCondition();

    /*
     * Checks if the thermal condition is different from the last time
     * setClockMode was called.
     */
    static bool isNewThermalCondition();

    /*
     * Requests client to reset the whole chip. This should only be called
     * on non-recoverable errors.
     *
     * Returns zero for success or negative errno for failure.
     */
    static int requestChipReset();
};

/* returns true if Easel is present in the system */
extern bool isEaselPresent();

#endif // ANDROID_EASELCONTROL_H
