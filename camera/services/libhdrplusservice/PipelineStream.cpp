//#define LOG_NDEBUG 0
#define LOG_TAG "PipelineStream"
#include <utils/Log.h>

#include <errno.h>

#include "PipelineStream.h"

namespace pbcamera {

PipelineStream::PipelineStream()
        : mConfig({}) {
}

PipelineStream::~PipelineStream() {
    std::unique_lock<std::mutex> lock(mApiLock);
    destroyLocked();
}

std::shared_ptr<PipelineStream> PipelineStream::newPipelineStream(
        const StreamConfiguration &config, int numBuffers) {
    std::shared_ptr<PipelineStream> stream = std::shared_ptr<PipelineStream>(new PipelineStream());
    if (stream == nullptr) {
        ALOGE("%s: Creating a pipeline stream instance failed.", __FUNCTION__);
        return nullptr;
    }
    status_t res = stream->create(config, numBuffers);
    if (res != 0) {
        ALOGE("%s: Creating a pipeline stream failed: %s (%d).", __FUNCTION__, strerror(-res),
                res);
        return nullptr;
    }

    return stream;
}

status_t PipelineStream::create(const StreamConfiguration &config, int numBuffers) {
    std::unique_lock<std::mutex> lock(mApiLock);

    if (mAllBuffers.size() > 0) {
        // Stream is already created.
        return -EEXIST;
    }

    for (int i = 0; i < numBuffers; i++) {
        std::unique_ptr<PipelineBuffer> buffer =
                std::make_unique<PipelineHeapBuffer>(shared_from_this(), config);
        if (buffer == nullptr) {
            ALOGE("%s: Creating a buffer instance failed.", __FUNCTION__);
            destroyLocked();
            return -EINVAL;
        }
        status_t res = buffer->allocate();
        if (res != 0) {
            ALOGE("%s: Allocating stream (%ux%u format %d with %d buffers) failed: %s (%d)",
                    __FUNCTION__, config.width, config.height, config.format, numBuffers,
                    strerror(-res), res);
            destroyLocked();
            return res;
        }

        mAvailableBuffers.push_back(buffer.get());
        mAllBuffers.push_back(std::move(buffer));
    }

    mConfig = config;
    ALOGV("%s: Allocated stream id %d res %ux%u format %d with %d buffers.", __FUNCTION__,
            config.id, config.width, config.height, config.format, numBuffers);

    return 0;
}

bool PipelineStream::hasConfig(const StreamConfiguration &config) const {
    std::unique_lock<std::mutex> lock(mApiLock);

    return mAvailableBuffers.size() > 0 && mConfig.equals(config);
}

void PipelineStream::destroyLocked() {
    mAllBuffers.clear();
    mAvailableBuffers.clear();
    mConfig = {};
}

status_t PipelineStream::getBuffer(PipelineBuffer **buffer, uint32_t timeoutMs) {
    if (buffer == nullptr) return -EINVAL;

    std::unique_lock<std::mutex> lock(mApiLock);
    // Wait until a buffer is available or it times out.
    if (mAvailableBufferCond.wait_for(lock, std::chrono::milliseconds(timeoutMs),
            [&] { return mAvailableBuffers.size() > 0; }) == false) {
        return -ETIMEDOUT;
    }

    *buffer = mAvailableBuffers[0];
    mAvailableBuffers.pop_front();
    return 0;
}

status_t PipelineStream::returnBuffer(PipelineBuffer *buffer) {
    if (buffer == nullptr) return -EINVAL;

    std::unique_lock<std::mutex> lock(mApiLock);

    // Reset buffer's block.
    buffer->resetPipelineBlock();
    mAvailableBuffers.push_back(buffer);
    mAvailableBufferCond.notify_one();

    // TODO: Need a way to signal a buffer is available for pipeline input stream.

    return 0;
}

int PipelineStream::getStreamId() const {
    return mConfig.id;
}

} // namespace pbcamera
