// #define LOG_NDEBUG 0
#define LOG_TAG "HdrPlusService"
#include <utils/Log.h>

#include "HdrPlusService.h"

namespace pbcamera {

HdrPlusService::HdrPlusService() : mMessengerToClient(nullptr) {
}

HdrPlusService::~HdrPlusService() {
    std::unique_lock<std::mutex> lock(mApiLock);
    stopLocked();
}

int HdrPlusService::start() {
    std::unique_lock<std::mutex> lock(mApiLock);
    if (mMessengerToClient != nullptr) return -EEXIST;

    // Connect to client messenger.
    mMessengerToClient = new MessengerToHdrPlusClient();
    int res = mMessengerToClient->connect(*this);
    if (res != 0) {
        stopLocked();
    }
    return res;
}

void HdrPlusService::stopLocked() {
    if (mMessengerToClient == nullptr) return;

    mMessengerToClient->disconnect();
    mMessengerToClient = nullptr;
    mExitCondition.notify_one();
}

int HdrPlusService::wait() {
    std::unique_lock<std::mutex> lock(mApiLock);
    if (mMessengerToClient == nullptr) return 0;

    mExitCondition.wait(lock);
    return 0;
}

int HdrPlusService::connect() {
    std::unique_lock<std::mutex> lock(mApiLock);

    // For now, just return 0 to acknowlege connection.
    return 0;
}

int HdrPlusService::configureStreams(const StreamConfiguration *inputConfig,
            const StreamConfiguration *outputConfigs, uint32_t numOutputConfigs) {
    std::unique_lock<std::mutex> lock(mApiLock);

    ALOGV("%s: input: %dx%d %d", __FUNCTION__, inputConfig->width,
            inputConfig->height, inputConfig->format);
    for (uint32_t i = 0; i < numOutputConfigs; i++) {
        ALOGV("%s: output: %dx%d %d", __FUNCTION__, outputConfigs[i].width,
            outputConfigs[i].height, outputConfigs[i].format);
    }

    // TODO: Implement stream configurations.

    return 0;
}

} // namespace pbcamera