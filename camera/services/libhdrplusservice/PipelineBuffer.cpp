//#define LOG_NDEBUG 0
#define LOG_TAG "PipelineBuffer"
#include <utils/Log.h>

#include <errno.h>
#include <stdlib.h>

#include <system/graphics.h>

#include "PipelineBuffer.h"
#include "PipelineStream.h"
#include "HdrPlusTypes.h"

namespace pbcamera {

PipelineBuffer::PipelineBuffer(const std::weak_ptr<PipelineStream> &stream,
        const StreamConfiguration &config)
        : mWidth(0),
          mHeight(0),
          mFormat(0),
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
    return mWidth;
}

int32_t PipelineBuffer::getHeight() const {
    return mHeight;
}

int32_t PipelineBuffer::getFormat() const {
    return mFormat;
}

int32_t PipelineBuffer::getStride() const {
    return mStride;
}

status_t PipelineBuffer::clear() {
    uint8_t *data = getData();
    switch (mFormat) {
        case HAL_PIXEL_FORMAT_RAW10:
        case HAL_PIXEL_FORMAT_RAW16:
            memset(data, kClearRawValue, getDataSize());
            return 0;
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        {
            uint32_t lumaSize = mStride * mHeight;
            // Clear luma
            memset(data, kClearLumaValue, lumaSize);
            // Clear chroma
            memset(data + lumaSize, kClearChromaValue, getDataSize() - lumaSize);
            return 0;
        }
        default:
            ALOGE("%s: Format %d not supported.", __FUNCTION__, mFormat);
            return -EINVAL;
    }
}

PipelineHeapBuffer::PipelineHeapBuffer(const std::weak_ptr<PipelineStream> &stream,
        const StreamConfiguration &config) :
    PipelineBuffer(stream, config) {
}

PipelineHeapBuffer::~PipelineHeapBuffer() {
}

status_t PipelineHeapBuffer::allocate() {
    // Check if buffer is already allocated.
    if (mData.size() != 0) return -EEXIST;

    size_t numBytes = 0;
    size_t stride = 0;

    // TODO: this should consider alignment requirements of QC and PB HW. b/31623156.
    switch (mRequestedConfig.format) {
        case HAL_PIXEL_FORMAT_RAW10:
            numBytes = mRequestedConfig.width * mRequestedConfig.height * 10 / 8;
            stride = mRequestedConfig.width * 10 / 8;
            break;
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            numBytes = mRequestedConfig.width * mRequestedConfig.height * 3 / 2;
            stride = mRequestedConfig.width;
            break;
        case HAL_PIXEL_FORMAT_RAW16:
            numBytes = mRequestedConfig.width * mRequestedConfig.height * 2;
            stride = mRequestedConfig.width * 2;
            break;
        default:
            ALOGE("%s: Format %d not supported.", __FUNCTION__, mRequestedConfig.format);
            return -EINVAL;
    }

    mData.resize(numBytes, 0);
    mWidth = mRequestedConfig.width;
    mHeight = mRequestedConfig.height;
    mFormat = mRequestedConfig.format;
    mStride = stride;

    return 0;
}

uint8_t* PipelineHeapBuffer::getData() {
    if (mData.size() == 0) {
        return nullptr;
    }

    return mData.data();
}

uint32_t PipelineHeapBuffer::getDataSize() const {
    return mData.size();
}

} // namespace pbcamera
