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

#include <stdint.h>

#ifndef ANDROID
// Supply the Android priority enum values for non-Android build
typedef enum android_LogPriority {
    ANDROID_LOG_UNKNOWN = 0,
    ANDROID_LOG_DEFAULT,    /* only for SetMinPriority() */
    ANDROID_LOG_VERBOSE,
    ANDROID_LOG_DEBUG,
    ANDROID_LOG_INFO,
    ANDROID_LOG_WARN,
    ANDROID_LOG_ERROR,
    ANDROID_LOG_FATAL,
    ANDROID_LOG_SILENT,     /* only for SetMinPriority(); must be last */
} android_LogPriority;
#endif

class EaselControlClient {
public:
    enum Camera { MAIN, FRONT };

    /*
     * Open an easelcontrol connection to Easel.  Initialize easelcomm
     * communications for the easelcontrol service.
     *
     * Returns zero for success or -1 for failure.
     */
    int open();
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
     * Start and/or configure one of the MIPI TX channels. This function will
     * block until Easel is powered.
     *
     * Returns zero for success or error code for failure.
     */
    static int startMipi(enum Camera camera, int rate);
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
};

class EaselControlServer {
public:
    /*
     * Open an easelcontrol connection.  Initialize easelcomm communications
     * for the easelcontrol service.
     *
     * Returns zero for success or -1 for failure.
     */
    int open();
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
     * Log to Android main log buffer.
     *
     * prio is a log priority such as ANDROID_LOG_INFO.
     * tag is the log tag string to use.
     * text is the string to log.
     */
    static void log(int prio, const char *tag, const char *text);
};

/* Convenience wrapper for EaselControlServer::log() */
extern void easelLog(int prio, const char *tag, const char *format, ...);

/* returns true if Easel is present in the system */
extern bool isEaselPresent();

#endif // ANDROID_EASELCONTROL_H
