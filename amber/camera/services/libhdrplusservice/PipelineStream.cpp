//#define LOG_NDEBUG 0
#define LOG_TAG "PipelineStream"
#include <log/log.h>

#include <errno.h>
#include <system/graphics.h>

#include "CaptureServiceConsts.h"
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
        ImxMemoryAllocatorHandle imxMemoryAllocatorHandle, const StreamConfiguration &config,
        int numBuffers) {
    std::shared_ptr<PipelineStream> stream = std::shared_ptr<PipelineStream>(new PipelineStream());
    if (stream == nullptr) {
        ALOGE("%s: Creating a pipeline stream instance failed.", __FUNCTION__);
        return nullptr;
    }
    status_t res = stream->create(imxMemoryAllocatorHandle, config, numBuffers);
    if (res != 0) {
        ALOGE("%s: Creating a pipeline stream failed: %s (%d).", __FUNCTION__, strerror(-res),
                res);
        return nullptr;
    }

    return stream;
}

status_t PipelineStream::create(ImxMemoryAllocatorHandle imxMemoryAllocatorHandle,
        const StreamConfiguration &config, int numBuffers) {
    std::unique_lock<std::mutex> lock(mApiLock);

    if (mAllBuffers.size() > 0) {
        // Stream is already created.
        return -EEXIST;
    }

    for (int i = 0; i < numBuffers; i++) {
        std::unique_ptr<PipelineImxBuffer> buffer =
                std::make_unique<PipelineImxBuffer>(shared_from_this(), config);
        if (buffer == nullptr) {
            ALOGE("%s: Creating a buffer instance failed.", __FUNCTION__);
            destroyLocked();
            return -EINVAL;
        }

        // Due to Easel memory limitation, do not allocate output buffers up-front. Output
        // buffers will be allocated when needed.

        mAvailableBuffers.push_back(buffer.get());
        mAllBuffers.push_back(std::move(buffer));
    }

    mConfig = config;
    ALOGV("%s: Allocated stream id %d res %ux%u format %d with %d buffers.", __FUNCTION__,
            config.id, config.image.width, config.image.height, config.image.format, numBuffers);

    return 0;
}

std::shared_ptr<PipelineStream> PipelineStream::newInputPipelineStream(
        const InputConfiguration &inputConfig, int numBuffers) {
    std::shared_ptr<PipelineStream> stream = std::shared_ptr<PipelineStream>(new PipelineStream());
    if (stream == nullptr) {
        ALOGE("%s: Creating an input pipeline stream instance failed.", __FUNCTION__);
        return nullptr;
    }
    status_t res = stream->createInput(inputConfig, numBuffers);
    if (res != 0) {
        ALOGE("%s: Creating an input pipeline stream failed: %s (%d).", __FUNCTION__,
                strerror(-res), res);
        return nullptr;
    }

    return stream;
}

status_t PipelineStream::createInput(const InputConfiguration &inputConfig, int numBuffers) {
    std::unique_lock<std::mutex> lock(mApiLock);
    if (mAllBuffers.size() > 0) {
        // Stream is already created.
        return -EEXIST;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    paintbox::MipiRxPort mipiRxPort = paintbox::MipiRxPort::RX0;
    bool busAligned = false;

    if (inputConfig.isSensorInput) {
        // Check the input is RAW10.
        if (inputConfig.sensorMode.format != HAL_PIXEL_FORMAT_RAW10) {
            ALOGE("%s: Only HAL_PIXEL_FORMAT_RAW10 is supported but sensor mode has %d",
                    __FUNCTION__, inputConfig.sensorMode.format);
            return -EINVAL;
        }

        width = inputConfig.sensorMode.pixelArrayWidth;
        height = inputConfig.sensorMode.pixelArrayHeight;
        busAligned = capture_service_consts::kBusAlignedStreamConfig;

        switch (inputConfig.sensorMode.cameraId) {
            case 0:
                mipiRxPort = paintbox::MipiRxPort::RX0;
                break;
            case 1:
                mipiRxPort = paintbox::MipiRxPort::RX1;
                break;
            default:
                ALOGE("%s: Camera ID (%u) is not supported.", __FUNCTION__,
                        inputConfig.sensorMode.cameraId);
                return -EINVAL;
        }
    } else {
        // Check the input is RAW10.
        if (inputConfig.streamConfig.image.format != HAL_PIXEL_FORMAT_RAW10) {
            ALOGE("%s: Only HAL_PIXEL_FORMAT_RAW10 is supported but input config has %d",
                    __FUNCTION__, inputConfig.streamConfig.image.format);
            return -EINVAL;
        }

        // Check there is 1 plane.
        if (inputConfig.streamConfig.image.planes.size() != 1) {
            ALOGE("%s: Only support 1 plane but input config has %lu planes",
                    __FUNCTION__, inputConfig.streamConfig.image.planes.size());
            return -EINVAL;
        }

        width = inputConfig.streamConfig.image.width;
        height = inputConfig.streamConfig.image.height;

        // Check each line has no padding.
        if (inputConfig.streamConfig.image.planes[0].stride != width * 10 / 8) {
            ALOGE("%s: Image width is %u but stride is %u",
                    __FUNCTION__, width, inputConfig.streamConfig.image.planes[0].stride);
            return -EINVAL;
        }
    }

    paintbox::MipiDataTypeCsi2 dataType = paintbox::RAW10;
    uint32_t bitsPerPixel = 10;

    std::vector<paintbox::CaptureStreamConfig> captureStreamConfigs = {
            { dataType, width, height, bitsPerPixel, busAligned }};

    paintbox::CaptureConfig captureConfig = { mipiRxPort,
            capture_service_consts::kMainImageVirtualChannelId,
            capture_service_consts::kCaptureFrameBufferFactoryTimeoutMs,
            captureStreamConfigs };

    // Create a capture frame buffer factory.
    mBufferFactory = paintbox::CaptureFrameBufferFactory::CreateInstance(captureConfig);
    if (mBufferFactory == nullptr) {
        ALOGE("%s: Failed to create a buffer factory.", __FUNCTION__);
        return -ENOMEM;
    }

    // Prepare stream configuration.
    StreamConfiguration config;
    if (inputConfig.isSensorInput) {
        config.image.width = width;
        config.image.height = height;
        config.image.format = inputConfig.sensorMode.format;

        PlaneConfiguration plane = {};
        plane.stride = config.image.width * bitsPerPixel / 8;
        plane.scanline = config.image.height;

        config.image.planes.push_back(plane);
    } else {
        config = inputConfig.streamConfig;
    }

    // Allocate the buffers using capture frame buffer factory.
    for (int i = 0; i < numBuffers; i++) {
        std::unique_ptr<PipelineCaptureFrameBuffer> buffer =
                std::make_unique<PipelineCaptureFrameBuffer>(shared_from_this(), config);
        if (buffer == nullptr) {
            ALOGE("%s: Creating a buffer instance failed.", __FUNCTION__);
            destroyLocked();
            return -EINVAL;
        }
        status_t res = buffer->allocate(mBufferFactory);
        if (res != 0) {
            ALOGE("%s: Allocating stream (%ux%u format %d with %d buffers) failed: %s (%d)",
                    __FUNCTION__, config.image.width, config.image.height, config.image.format,
                    numBuffers, strerror(-res), res);
            destroyLocked();
            return res;
        }

        mAvailableBuffers.push_back(buffer.get());
        mAllBuffers.push_back(std::move(buffer));
    }

    mConfig = config;
    ALOGV("%s: Allocated stream id %d res %ux%u format %d with %d buffers.", __FUNCTION__,
            config.id, config.image.width, config.image.height, config.image.format, numBuffers);

    return 0;
}

bool PipelineStream::hasConfig(const StreamConfiguration &config) const {
    std::unique_lock<std::mutex> lock(mApiLock);

    return mAvailableBuffers.size() > 0 && mConfig == config;
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
