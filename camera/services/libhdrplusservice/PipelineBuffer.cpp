//#define LOG_NDEBUG 0
#define LOG_TAG "PipelineBuffer"
#include <log/log.h>

#include <errno.h>
#include <stdlib.h>

#include <system/graphics.h>

#include "PipelineBuffer.h"
#include "PipelineStream.h"
#include "HdrPlusTypes.h"

namespace pbcamera {

PipelineBuffer::PipelineBuffer(const std::weak_ptr<PipelineStream> &stream,
        const StreamConfiguration &config)
        : mAllocatedConfig({}),
          mRequestedConfig(config),
          mStream(stream) {
}

PipelineBuffer::~PipelineBuffer() {
}

std::weak_ptr<PipelineStream> PipelineBuffer::getStream() const {
    return mStream;
}

void PipelineBuffer::setPipelineBlock(std::weak_ptr<PipelineBlock> block) {
    mBlock = block;
}

void PipelineBuffer::resetPipelineBlock() {
    mBlock.reset();
}

std::weak_ptr<PipelineBlock> PipelineBuffer::getPipelineBlock() const {
    return mBlock;
}

int32_t PipelineBuffer::getWidth() const {
    return mAllocatedConfig.image.width;
}

int32_t PipelineBuffer::getHeight() const {
    return mAllocatedConfig.image.height;
}

int32_t PipelineBuffer::getFormat() const {
    return mAllocatedConfig.image.format;
}

int32_t PipelineBuffer::getStride(uint32_t planeNum) const {
    return mAllocatedConfig.image.planes.size() > planeNum ?
           mAllocatedConfig.image.planes[planeNum].stride : 0;
}

status_t PipelineBuffer::clear() {
    uint8_t *data = getPlaneData(0);
    switch (mAllocatedConfig.image.format) {
        case HAL_PIXEL_FORMAT_RAW10:
        case HAL_PIXEL_FORMAT_RAW16:
            memset(data, kClearRawValue, getDataSize());
            return 0;
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        {
            uint32_t lumaSize = mAllocatedConfig.image.planes[0].stride *
                                mAllocatedConfig.image.height;
            // Clear luma
            memset(data, kClearLumaValue, lumaSize);

            data = getPlaneData(1);
            uint32_t chromaSize = mAllocatedConfig.image.planes[1].stride *
                                  mAllocatedConfig.image.height / 2;
            // Clear chroma
            memset(data, kClearChromaValue, chromaSize);
            return 0;
        }
        default:
            ALOGE("%s: Format %zd not supported.", __FUNCTION__, mAllocatedConfig.image.format);
            return -EINVAL;
    }
}

status_t PipelineBuffer:: validatePlaneConfig(const ImageConfiguration &image, uint32_t planeNum) {
    if (planeNum >= image.planes.size()) {
        ALOGE("%s: Validating plane %d failed because it only has %zu planes.", __FUNCTION__,
                planeNum, image.planes.size());
        return -EINVAL;
    }

    // Assumes the number of planes for the format has been validated previously.
    const PlaneConfiguration& plane = image.planes[planeNum];

    uint32_t minStride;
    switch (image.format) {
        case HAL_PIXEL_FORMAT_RAW10:
            minStride = image.width * 10 / 8;
            break;
        case HAL_PIXEL_FORMAT_RAW16:
            minStride = image.width * 2;
            break;
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            minStride = image.width;
            break;
        default:
            ALOGE("%s: Format %zd not supported.", __FUNCTION__, image.format);
            return -EINVAL;
    }

    if (plane.stride < minStride) {
        ALOGE("%s: Plane stride %d is smaller than minimal stride %d.", __FUNCTION__, plane.stride,
                minStride);
        return -EINVAL;
    }

    uint32_t minScanline;
    minScanline = image.height; // RAW10, RAW16, Y planes.
    if (image.format == HAL_PIXEL_FORMAT_YCrCb_420_SP && planeNum == 1) {
        minScanline /= 2; // UV plane.
    }

    if (plane.scanline < minScanline) {
        ALOGE("%s: Plane scanline %d is smaller than minimal scanline %d.", __FUNCTION__,
                plane.scanline, minScanline);
        return -EINVAL;
    }

    return 0;
}

status_t PipelineBuffer::validateConfig(const StreamConfiguration &config) {
    size_t expectedNumPlanes;

    // Get the expect number of planes given the format.
    switch (config.image.format) {
        case HAL_PIXEL_FORMAT_RAW10:
        case HAL_PIXEL_FORMAT_RAW16:
            expectedNumPlanes = 1;
            break;

        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            expectedNumPlanes = 2;
            break;
        default:
            ALOGE("%s: Format %zd not supported.", __FUNCTION__, mRequestedConfig.image.format);
            return -EINVAL;
    }

    // Verify number of planes is correct.
    if (config.image.planes.size() != expectedNumPlanes) {
        ALOGE("%s: Expecting %zu planes for format %zd but got %zu planes.", __FUNCTION__,
                expectedNumPlanes, config.image.format, config.image.planes.size());
        return -EINVAL;
    }

    // Validate each plane
    status_t res;
    for (size_t i = 0; i < config.image.planes.size(); i++) {
        res = validatePlaneConfig(config.image, i);
        if (res != 0) {
            ALOGE("%s: Validating plane %zu failed.", __FUNCTION__, i);
            return -EINVAL;
        }
    }

    return 0;
}

/********************************************
 * PipelineHeapBuffer implementation starts.
 ********************************************/
PipelineHeapBuffer::PipelineHeapBuffer(const std::weak_ptr<PipelineStream> &stream,
        const StreamConfiguration &config) :
    PipelineBuffer(stream, config),
    mData(nullptr),
    mDataSize(0) {
}

PipelineHeapBuffer::~PipelineHeapBuffer() {
    if (mData != nullptr) {
        free(mData);
        mData = nullptr;
        mDataSize = 0;
    }
}

status_t PipelineHeapBuffer::allocate() {
    // Check if buffer is already allocated.
    if (mData != nullptr) return -EEXIST;

    status_t res = validateConfig(mRequestedConfig);
    if (res != 0) {
        ALOGE("%s: Requested configuration is invalid: %s (%d).", __FUNCTION__, strerror(-res),
                res);
        return res;
    }

    size_t numBytes = mRequestedConfig.image.padding;

    for (auto plane : mRequestedConfig.image.planes) {
        numBytes += plane.stride * plane.scanline;
    }

    mData = (uint8_t*)malloc(numBytes);
    if (mData == nullptr) {
        ALOGE("%s: Allocating buffer failed.", __FUNCTION__);
        return -ENOMEM;
    }

    mDataSize = numBytes;
    mAllocatedConfig = mRequestedConfig;
    return 0;
}

uint8_t* PipelineHeapBuffer::getPlaneData(uint32_t planeNum) {
    if (mData == nullptr || planeNum >= mAllocatedConfig.image.planes.size()) {
        return nullptr;
    }

    uint32_t planeOffset = 0;
    for (uint32_t i = 0; i < planeNum; i++) {
        planeOffset += (mAllocatedConfig.image.planes[i].stride *
                        mAllocatedConfig.image.planes[i].scanline);
    }

    return mData + planeOffset;
}

uint32_t PipelineHeapBuffer::getDataSize() const {
    return mDataSize;
}

status_t PipelineHeapBuffer::lockData() {
    // Do nothing.
    return 0;
}

void PipelineHeapBuffer::unlockData() {
    // Do nothing.
}

/***************************************************
 * PipelineCaptureFrameBuffer implementation starts.
 ***************************************************/
PipelineCaptureFrameBuffer::PipelineCaptureFrameBuffer(const std::weak_ptr<PipelineStream> &stream,
        const StreamConfiguration &config) :
        PipelineBuffer(stream, config),
        mLockedData(nullptr) {
}

status_t PipelineCaptureFrameBuffer::allocate() {
    ALOGE("%s: Use CaptureFrameBufferFactory to allocate capture frame buffers.", __FUNCTION__);
    return -EINVAL;
}

status_t PipelineCaptureFrameBuffer::allocate(
        std::unique_ptr<CaptureFrameBufferFactory> &bufferFactory) {
    // Check if buffer is already allocated.
    if (mCaptureFrameBuffer != nullptr) return -EEXIST;

    if (bufferFactory == nullptr) {
        ALOGE("%s: Buffer factory is nullptr", __FUNCTION__);
        return -EINVAL;
    }

    status_t res = validateConfig(mRequestedConfig);
    if (res != 0) {
        ALOGE("%s: Requested configuration is invalid: %s (%d).", __FUNCTION__, strerror(-res),
                res);
        return res;
    }

    mCaptureFrameBuffer = bufferFactory->Create();
    if (mCaptureFrameBuffer == nullptr) {
        ALOGE("%s: Failed to allocate a capture frame buffer.", __FUNCTION__);
        return -ENOMEM;
    }

    mAllocatedConfig = mRequestedConfig;
    return 0;
}

uint8_t* PipelineCaptureFrameBuffer::getPlaneData(uint32_t planeNum) {
    if (mCaptureFrameBuffer == nullptr) {
        ALOGE("%s: Capture frame buffer is nullptr.", __FUNCTION__);
        return nullptr;
    } else if(planeNum >= mAllocatedConfig.image.planes.size()) {
        ALOGE("%s: Getting plane %d but the image has %lu planes.", __FUNCTION__, planeNum,
                mAllocatedConfig.image.planes.size());
        return nullptr;
    } else if (mLockedData == nullptr) {
        ALOGE("%s: Data is not locked.", __FUNCTION__);
        return nullptr;
    }

    uint32_t planeOffset = 0;
    for (uint32_t i = 0; i < planeNum; i++) {
        planeOffset += (mAllocatedConfig.image.planes[i].stride *
                        mAllocatedConfig.image.planes[i].scanline);
    }

    return static_cast<uint8_t*>(mLockedData) + planeOffset;
}

uint32_t PipelineCaptureFrameBuffer::getDataSize() const {
    uint32_t size = 0;
    for (uint32_t i = 0; i < mAllocatedConfig.image.planes.size(); i++) {
        size += (mAllocatedConfig.image.planes[i].stride *
                 mAllocatedConfig.image.planes[i].scanline);
    }
    return size;
}

status_t PipelineCaptureFrameBuffer::lockData() {
    if (mCaptureFrameBuffer == nullptr) {
        ALOGE("%s: Capture frame buffer is nullptr.", __FUNCTION__);
        return -EINVAL;
    }

    if (mLockedData != nullptr) {
        return 0;
    }

    const std::vector<int> dataTypes = mCaptureFrameBuffer->GetDataTypeList();
    if (dataTypes.size() != 1) {
        ALOGE("%s: This buffer has %lu data types. Only 1 is supported.", __FUNCTION__,
                dataTypes.size());
        return -EINVAL;
    }

    CaptureError err = mCaptureFrameBuffer->LockFrameData(dataTypes[0], &mLockedData);
    if (err != CaptureError::SUCCESS) {
        ALOGE("%s: Locking frame data failed: %s (%d)", __FUNCTION__,
                GetCaptureErrorDesc(err),  err);
        mLockedData = nullptr;
    }

    return 0;
}

void PipelineCaptureFrameBuffer::unlockData() {
    if (mLockedData == nullptr)
        return;

    const std::vector<int> dataTypes = mCaptureFrameBuffer->GetDataTypeList();
    if (dataTypes.size() != 1) {
        ALOGE("%s: This buffer has %lu data types. Only 1 is supported.", __FUNCTION__,
                dataTypes.size());
        return;
    }

    CaptureError err = mCaptureFrameBuffer->UnlockFrameData(dataTypes[0]);
    if (err != CaptureError::SUCCESS) {
        ALOGE("%s: Unlocking frame data failed: err=%d", __FUNCTION__, err);
    }

    mLockedData = nullptr;
}

CaptureFrameBuffer* PipelineCaptureFrameBuffer::getCaptureFrameBuffer() {
    return mCaptureFrameBuffer.get();
}

} // namespace pbcamera
