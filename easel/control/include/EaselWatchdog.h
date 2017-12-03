#ifndef __EASEL_WATCHDOG_H__
#define __EASEL_WATCHDOG_H__

#include <chrono>
#include <stdio.h>
#include <unistd.h>

/*
 * This class abstracts a typical "watchdog" that will "bite" (via a callback)
 * if the dog is not "pet" within a specified period.
 */
class EaselWatchdog {
public:
    /*
     * Starts watchdog.
     *
     * period - latency after a "pet" before bite callback is called.
     *
     * Returns 0 on success or a negative error for failure.
     */
    int start(const std::chrono::duration<int> period);

    /*
     * Pet watchdog.
     *
     * Returns 0 on success or a negative error for failure.
     */
    int pet();

    /*
     * Stops watchdog.
     *
     * Returns 0 on success or a negative error for failure.
     */
    int stop();

    /*
     * Set the bite callback.
     *
     * Returns 0 on success or a negative error for failure.
     */
    int setBiteCallback(std::function<void()> callback);
};

#endif // __EASEL_WATCHDOG_H__
