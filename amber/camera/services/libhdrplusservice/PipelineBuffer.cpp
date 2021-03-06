//#define LOG_NDEBUG 0
#define LOG_TAG "PipelineBuffer"
#include <log/log.h>

#include <errno.h>
#include <stdlib.h>

#include <system/graphics.h>

#include "PipelineBuffer.h"
#include "PipelineStream.h"
#include "HdrPlusTypes.h"
#include "third_party/halide/paintbox/src/runtime/imx.h"

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
    return mRequestedConfig.image.width;
}

int32_t PipelineBuffer::getHeight() const {
    return mRequestedConfig.image.height;
}

int32_t PipelineBuffer::getFormat() const {
    return mRequestedConfig.image.format;
}

int32_t PipelineBuffer::getStride(uint32_t planeNum) const {
    return mAllocatedConfig.image.planes.size() > planeNum ?
           mAllocatedConfig.image.planes[planeNum].stride : 0;
}

int PipelineBuffer::getFd() {
    ALOGE("%s: Not supported.", __FUNCTION__);
    return -1;
}

status_t PipelineBuffer::clear() {
    uint8_t *data = getPlaneData(0);
    switch (mAllocatedConfig.image.format) {
        case HAL_PIXEL_FORMAT_RAW10:
        case HAL_PIXEL_FORMAT_RAW16:
            memset(data, kClearRawValue, getDataSize());
            return 0;
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        case HAL_PIXEL_FORMAT_YCbCr_420_SP:
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
            ALOGE("%s: Format %d not supported.", __FUNCTION__, mAllocatedConfig.image.format);
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
        case HAL_PIXEL_FORMAT_YCbCr_420_SP:
            minStride = image.width;
            break;
        default:
            ALOGE("%s: Format %d not supported.", __FUNCTION__, image.format);
            return -EINVAL;
    }

    if (plane.stride < minStride) {
        ALOGE("%s: Plane stride %d is smaller than minimal stride %d.", __FUNCTION__, plane.stride,
                minStride);
        return -EINVAL;
    }

    uint32_t minScanline;
    minScanline = image.height; // RAW10, RAW16, Y planes.
    if ((image.format == HAL_PIXEL_FORMAT_YCrCb_420_SP ||
            image.format == HAL_PIXEL_FORMAT_YCbCr_420_SP ) && planeNum == 1) {
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
        case HAL_PIXEL_FORMAT_YCbCr_420_SP:
            expectedNumPlanes = 2;
            break;
        default:
            ALOGE("%s: Format %d not supported.", __FUNCTION__, mRequestedConfig.image.format);
            return -EINVAL;
    }

    // Verify number of planes is correct.
    if (config.image.planes.size() != expectedNumPlanes) {
        ALOGE("%s: Expecting %zu planes for format %d but got %zu planes.", __FUNCTION__,
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
 * PipelineImxBuffer implementation starts.
 ********************************************/
PipelineImxBuffer::PipelineImxBuffer(const std::weak_ptr<PipelineStream> &stream,
        const StreamConfiguration &config) :
    PipelineBuffer(stream, config),
    mImxDeviceBufferHandle(nullptr),
    mLockedData(nullptr),
    mDataSize(0) {
}

PipelineImxBuffer::~PipelineImxBuffer() {
    destroy();
}

status_t PipelineImxBuffer::allocate() {
    ALOGE("%s: Use ImxMemoryAllocatorHandle to allocate capture frame buffers.", __FUNCTION__);
    return -EINVAL;
}

void PipelineImxBuffer::destroy() {
    if (mImxDeviceBufferHandle != nullptr) {
        ImxDeleteDeviceBuffer(mImxDeviceBufferHandle);
        mImxDeviceBufferHandle = nullptr;
    }

    mYuvImage = nullptr;
    mLockedData = nullptr;
    mDataSize = 0;
    mAllocatedConfig = {};
}

status_t PipelineImxBuffer::allocate(ImxMemoryAllocatorHandle imxMemoryAllocatorHandle) {
    // Check if buffer is already allocated.
    if (mImxDeviceBufferHandle != nullptr || mYuvImage != nullptr) return -EEXIST;

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

    ImxError err = ImxCreateDeviceBufferManaged(imxMemoryAllocatorHandle,
            numBytes,
            kImxDefaultDeviceBufferAlignment,
            kImxDefaultDeviceBufferHeap,
            /*flags*/0,
            &mImxDeviceBufferHandle);
    if (err != 0) {
        ALOGE("%s: Allocate %zu bytes failed: %d", __FUNCTION__, numBytes, err);
        return -ENOMEM;
    }

    mDataSize = numBytes;
    mAllocatedConfig = mRequestedConfig;
    return 0;
}

uint8_t* PipelineImxBuffer::getPlaneData(uint32_t planeNum) {
    if (mImxDeviceBufferHandle == nullptr && mYuvImage == nullptr) {
        ALOGE("%s: Buffer is not allocated.", __FUNCTION__);
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

int PipelineImxBuffer::getFd() {
    ImxDeviceBufferHandle handle = nullptr;
    int fd = -1;

    if (mYuvImage != nullptr) {
        uint64_t offset = 0;
        ImxError err = ImxGetDeviceBufferFromAddress(&mYuvImage->luma_read_view().at(0, 0, 0),
                &handle, &offset);
        if (err != 0) {
            return -1;
        }
    } else {
        handle = mImxDeviceBufferHandle;
    }

    ImxShareDeviceBuffer(handle, &fd);
    return fd;
}


uint32_t PipelineImxBuffer::getDataSize() const {
    return mDataSize;
}

status_t PipelineImxBuffer::lockData() {
    if (mImxDeviceBufferHandle == nullptr && mYuvImage == nullptr) {
        ALOGE("%s: Buffer is not allocated.", __FUNCTION__);
        return -EINVAL;
    }

    if (mLockedData != nullptr) {
        return 0;
    }

    if (mYuvImage != nullptr) {
        mLockedData = static_cast<void*>(&mYuvImage->luma_write_view().at(0, 0, 0));
    } else {
        ImxError err = ImxLockDeviceBuffer(mImxDeviceBufferHandle, &mLockedData);
        if (err != 0) {
            ALOGE("%s: Locking buffer failed: %d", __FUNCTION__, err);
            mLockedData = nullptr;
            return -ENOMEM;
        }
    }

    return 0;
}

void PipelineImxBuffer::unlockData() {
    if (mLockedData == nullptr)
        return;

    if (mImxDeviceBufferHandle != nullptr) {
        ImxError err = ImxUnlockDeviceBuffer(mImxDeviceBufferHandle);
        if (err != 0) {
            ALOGE("%s: Unlocking buffer failed: %d", __FUNCTION__, err);
            return;
        }

    }

    mLockedData = nullptr;
}

bool PipelineImxBuffer::attachable(const std::unique_ptr<gcam::YuvImage> &yuvImage) {
    if (yuvImage == nullptr) {
        return false;
    }
    switch(yuvImage->yuv_format()) {
        case gcam::YuvFormat::kNv12:
            if (mRequestedConfig.image.format != HAL_PIXEL_FORMAT_YCbCr_420_SP) {
                return false;
            }
            break;
        case gcam::YuvFormat::kNv21:
            if (mRequestedConfig.image.format != HAL_PIXEL_FORMAT_YCrCb_420_SP) {
                return false;
            }
            break;
        default:
            return false;
    }

    const gcam::InterleavedReadViewU8& lumaReadView = yuvImage->luma_read_view();
    const gcam::InterleavedReadViewU8& chromaReadView = yuvImage->chroma_read_view();

    if (mRequestedConfig.image.width != static_cast<uint32_t>(lumaReadView.width()) ||
        mRequestedConfig.image.height != static_cast<uint32_t>(lumaReadView.height())) {
        return false;
    }

    if (mRequestedConfig.image.planes.size() != 2) {
        return false;
    }

    if (mRequestedConfig.image.planes[0].stride != lumaReadView.y_stride()) {
        return false;
    }

    int64_t lumaPlaneSize = (int64_t)&chromaReadView.at(0, 0, 0) -
                            (int64_t)&lumaReadView.at(0, 0, 0);

    if (mRequestedConfig.image.planes[0].scanline !=
            lumaPlaneSize / lumaReadView.y_stride()) {
        return false;
    }

    if (mRequestedConfig.image.planes[1].stride != chromaReadView.y_stride()) {
        return false;
    }

    // Requested chroma scanline must be larger than chroma height.
    if (mRequestedConfig.image.planes[1].scanline <
            static_cast<uint32_t>(chromaReadView.height())) {
        return false;
    }

    return true;
}

status_t PipelineImxBuffer::attachImage(std::unique_ptr<gcam::YuvImage> *yuvImage) {
    if (yuvImage == nullptr) return -EINVAL;

    if (mYuvImage != nullptr || mImxDeviceBufferHandle != nullptr) {
        ALOGE("%s: Buffer is allocated.", __FUNCTION__);
        return -EEXIST;
    }

    if (!attachable(*yuvImage)) {
        ALOGE("%s: Not attachable.", __FUNCTION__);
        return -EINVAL;
    }

    mYuvImage = std::move(*yuvImage);
    mDataSize = (int64_t)&mYuvImage->chroma_read_view().at(0, 0, 0) -
                    (int64_t)&mYuvImage->luma_read_view().at(0, 0, 0) +
                    mYuvImage->chroma_read_view().sample_array_size();
    mAllocatedConfig = mRequestedConfig;

    // Update the chroma scanline to the height the chroma plane because we can't query
    // YuvImage's scanline and we can't be sure that YuvImage's scanline is the same as
    // the requested configuration.
    mAllocatedConfig.image.planes[1].scanline = mYuvImage->chroma_read_view().height();

    return 0;
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

void PipelineCaptureFrameBuffer::destroy() {
    mCaptureFrameBuffer = nullptr;
    mLockedData = nullptr;
    mAllocatedConfig = {};
}

status_t PipelineCaptureFrameBuffer::allocate(
        std::unique_ptr<paintbox::CaptureFrameBufferFactory> &bufferFactory) {
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

    // Check there is only 1 plane.
    if (mRequestedConfig.image.planes.size() != 1) {
        ALOGE("%s: Requested %ld planes, only support 1 plane.", __FUNCTION__,
                mRequestedConfig.image.planes.size());
        return -EINVAL;
    }

    mCaptureFrameBuffer = bufferFactory->Create();
    if (mCaptureFrameBuffer == nullptr) {
        ALOGE("%s: Failed to allocate a capture frame buffer.", __FUNCTION__);
        return -ENOMEM;
    }

    mAllocatedConfig = mRequestedConfig;

    // Update stride
    const std::vector<int> dataTypes = mCaptureFrameBuffer->GetDataTypeList();
    if (dataTypes.size() != 1) {
        ALOGE("%s: This buffer has %lu data types. Only 1 is supported.", __FUNCTION__,
                dataTypes.size());
        return -EINVAL;
    }

    // Assume only 1 plane.
    mAllocatedConfig.image.planes[0].stride =
            mCaptureFrameBuffer->GetRowStrideBytes(dataTypes[0]);
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

int PipelineCaptureFrameBuffer::getFd() {
    ALOGE("%s: Getting FD of a capture frame buffer is not supported.", __FUNCTION__);
    return -1;
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

    paintbox::CaptureError err = mCaptureFrameBuffer->LockFrameData(dataTypes[0], &mLockedData);
    if (err != paintbox::CaptureError::SUCCESS) {
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

    paintbox::CaptureError err = mCaptureFrameBuffer->UnlockFrameData(dataTypes[0]);
    if (err != paintbox::CaptureError::SUCCESS) {
        ALOGE("%s: Unlocking frame data failed: err=%d", __FUNCTION__, err);
    }

    mLockedData = nullptr;
}

paintbox::CaptureFrameBuffer* PipelineCaptureFrameBuffer::getCaptureFrameBuffer() {
    return mCaptureFrameBuffer.get();
}

} // namespace pbcamera
