#define LOG_TAG "EaselTimer"

#include <log/log.h>
#include <thread>

#include "EaselTimer.h"

void EaselTimer::timer(std::chrono::milliseconds period, std::function<void()> callback, bool fireOnce)
{
    std::unique_lock<std::mutex> lock(mMutex);
    while (!mStopFlag) {
        if (mCondition.wait_for(lock, period) == std::cv_status::timeout) {
            ALOGV("%s: timer expires", __FUNCTION__);
            callback();
            if (fireOnce) {
                break;
            }
        }
    }
}

int EaselTimer::start(std::chrono::milliseconds period, std::function<void()> callback, bool fireOnce)
{
    ALOGV("%s", __FUNCTION__);

    if (mThread.joinable()) {
        ALOGE("%s: timer is already running", __FUNCTION__);
        return -EBUSY;
    }

    if (!callback) {
        ALOGE("%s: cannot start timer with null callback", __FUNCTION__);
        return -EINVAL;
    }

    mStopFlag = false;
    mThread = std::thread(&EaselTimer::timer, this, period, callback, fireOnce);

    return 0;
}

int EaselTimer::restart()
{
    ALOGV("%s", __FUNCTION__);

    if (!mThread.joinable()) {
        ALOGE("%s: no timer running", __FUNCTION__);
        return -ENODEV;
    }

    mCondition.notify_all();

    return 0;
}

int EaselTimer::stop()
{
    ALOGV("%s", __FUNCTION__);

    if (!mThread.joinable()) {
        ALOGE("%s: no timer running", __FUNCTION__);
        return -ENODEV;
    }

    {
        std::unique_lock<std::mutex> lock(mMutex);
        mStopFlag = true;
    }
    mCondition.notify_all();
    mThread.join();

    return 0;
}
