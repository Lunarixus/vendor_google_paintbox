//#define LOG_NDEBUG 0
#define LOG_TAG "HdrPlusService"
#include <utils/Log.h>

#include "HdrPlusPipeline.h"
#include "HdrPlusService.h"

namespace pbcamera {

HdrPlusService::HdrPlusService() {
}

HdrPlusService::~HdrPlusService() {
    std::unique_lock<std::mutex> lock(mApiLock);
    stopLocked();
}

status_t HdrPlusService::start() {
    std::unique_lock<std::mutex> lock(mApiLock);
    if (mMessengerToClient != nullptr) return -EEXIST;

    // Connect to client messenger.
    mMessengerToClient = std::make_shared<MessengerToHdrPlusClient>();
    if (mMessengerToClient == nullptr) {
        ALOGE("%s: Creating a MessengerToHdrPlusClient instance failed.", __FUNCTION__);
        return -ENODEV;
    }

    status_t res = mMessengerToClient->connect(*this);
    if (res != 0) {
        ALOGE("%s: Connecting to messenger failed: %s (%d).", __FUNCTION__, strerror(-res), res);
        stopLocked();
        mMessengerToClient = nullptr;
        return -ENODEV;
    }

    // Create a pipeline
    mPipeline = HdrPlusPipeline::newPipeline(mMessengerToClient);
    return 0;
}

void HdrPlusService::stopLocked() {
    if (mMessengerToClient == nullptr) return;

    mMessengerToClient->disconnect();
    mMessengerToClient = nullptr;
    mExitCondition.notify_one();
}

void HdrPlusService::wait() {
    std::unique_lock<std::mutex> lock(mApiLock);
    if (mMessengerToClient == nullptr) return;

    mExitCondition.wait(lock);
    return;
}

status_t HdrPlusService::connect() {
    std::unique_lock<std::mutex> lock(mApiLock);

    // For now, just return 0 to acknowlege connection.
    return 0;
}

status_t HdrPlusService::configureStreams(const StreamConfiguration &inputConfig,
            const std::vector<StreamConfiguration> &outputConfigs) {
    std::unique_lock<std::mutex> lock(mApiLock);

    if (mPipeline == nullptr) return -ENODEV;

    // Configure the pipeline.
    return mPipeline->configure(inputConfig, outputConfigs);
}

status_t HdrPlusService::submitCaptureRequest(const CaptureRequest &request) {
    ALOGV("%s", __FUNCTION__);

    if (mPipeline == nullptr) return -ENODEV;

    return mPipeline->submitCaptureRequest(request);
}

} // namespace pbcamera