#define LOG_TAG "EaselWatchdog"

#include <log/log.h>
#include <thread>

#include "EaselWatchdog.h"

std::function<void()> biteCallback;

// Thread waiting for timer to expire before bite
std::thread thread;

// Lock used for synchronizing the waiting thread with the application thread.
std::mutex mutex;

// Condition used to notify thread.
std::condition_variable condition;

// Flag for coordinating with the thread
bool stopFlag;

void patrol(std::chrono::duration<int> period)
{
    std::unique_lock<std::mutex> lock(mutex);
    while (!stopFlag) {
        if ((condition.wait_for(lock, period) == std::cv_status::timeout) && biteCallback) {
            ALOGE("%s: watchdog bite!", __FUNCTION__);
            biteCallback();
        }
    }
}

int EaselWatchdog::start(std::chrono::duration<int> period)
{
    ALOGV("%s", __FUNCTION__);

    if (thread.joinable()) {
        return -EBUSY;
    }

    stopFlag = false;
    thread = std::thread(patrol, period);

    return 0;
}

int EaselWatchdog::pet()
{
    ALOGV("%s", __FUNCTION__);

    if (!thread.joinable()) {
        return -ENOENT;
    }

    condition.notify_all();

    return 0;
}

int EaselWatchdog::stop()
{
    ALOGV("%s", __FUNCTION__);

    if (thread.joinable()) {
        {
            std::unique_lock<std::mutex> lock(mutex);
            stopFlag = true;
        }
        condition.notify_all();
        thread.join();
    }

    return 0;
}

int EaselWatchdog::setBiteCallback(std::function<void()> callback)
{
    biteCallback = callback;

    return 0;
}
