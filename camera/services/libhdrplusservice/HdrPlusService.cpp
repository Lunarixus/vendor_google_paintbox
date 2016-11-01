//#define LOG_NDEBUG 0
#define LOG_TAG "HdrPlusService"
#include "Log.h"

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

    // Opening Easel Control
    status_t res = mEaselControl.open();
    if (res != 0) {
        ALOGE("%s: Opening Easel Control failed: %s (%d).", __FUNCTION__, strerror(-errno), errno);
        stopLocked();
        return -ENODEV;
    }

    // Connect to client messenger.
    mMessengerToClient = std::make_shared<MessengerToHdrPlusClient>();
    if (mMessengerToClient == nullptr) {
        ALOGE("%s: Creating a MessengerToHdrPlusClient instance failed.", __FUNCTION__);
        stopLocked();
        return -ENODEV;
    }

    res = mMessengerToClient->connect(*this);
    if (res != 0) {
        ALOGE("%s: Connecting to messenger failed: %s (%d).", __FUNCTION__, strerror(-res), res);
        stopLocked();
        mMessengerToClient = nullptr;
        return -ENODEV;
    }

    return 0;
}

void HdrPlusService::stopLocked() {
    if (mMessengerToClient == nullptr) return;

    mMessengerToClient->disconnect();
    mMessengerToClient = nullptr;
    mEaselControl.close();
    mExitCondition.notify_one();
}

void HdrPlusService::wait() {
    std::unique_lock<std::mutex> lock(mApiLock);
    if (mMessengerToClient == nullptr) return;

    mExitCondition.wait(lock);
}

status_t HdrPlusService::connect() {
    ALOGV("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mApiLock);

    // Create a pipeline
    if (mPipeline != nullptr) {
        ALOGE("%s: Already connected.", __FUNCTION__);
        return -EEXIST;
    }

    mPipeline = HdrPlusPipeline::newPipeline(mMessengerToClient);
    ALOGD("%s: Connected.", __FUNCTION__);

    return 0;
}

void HdrPlusService::disconnect() {
    ALOGV("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mApiLock);
    if (mPipeline == nullptr) return;

    mPipeline = nullptr;
    ALOGD("%s: Disconnected.", __FUNCTION__);
}

status_t HdrPlusService::setStaticMetadata(const StaticMetadata& metadata) {
    std::unique_lock<std::mutex> lock(mApiLock);

    if (mPipeline == nullptr) return -ENODEV;

    // Configure the pipeline.
    return mPipeline->setStaticMetadata(metadata);
}

status_t HdrPlusService::configureStreams(const InputConfiguration &inputConfig,
            const std::vector<StreamConfiguration> &outputConfigs) {
    std::unique_lock<std::mutex> lock(mApiLock);

    if (mPipeline == nullptr) return -ENODEV;

    // Configure the pipeline.
    return mPipeline->configure(inputConfig, outputConfigs);
}

status_t HdrPlusService::submitCaptureRequest(const CaptureRequest &request) {
    ALOGV("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mApiLock);

    if (mPipeline == nullptr) {
        ALOGE("%s: Not connected.", __FUNCTION__);
        return -ENODEV;
    }

    return mPipeline->submitCaptureRequest(request);
}

void HdrPlusService::notifyDmaInputBuffer(const DmaImageBuffer &dmaInputBuffer,
        int64_t mockingEaselTimestampNs) {
    ALOGV("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mApiLock);

    if (mPipeline == nullptr) {
        ALOGE("%s: Not connected. Dropping an input buffer.", __FUNCTION__);
        return;
    }

    mPipeline->notifyDmaInputBuffer(dmaInputBuffer, mockingEaselTimestampNs);
}

void HdrPlusService::notifyFrameMetadata(const FrameMetadata &metadata) {
    ALOGV("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mApiLock);

    if (mPipeline == nullptr) {
        ALOGE("%s: Not connected.", __FUNCTION__);
        return;
    }

    mPipeline->notifyFrameMetadata(metadata);
}

} // namespace pbcamera
