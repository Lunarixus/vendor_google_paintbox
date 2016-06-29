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

    // TODO: this should consider alignment requirements of QC and PB HW.
    switch (mRequestedConfig.format) {
        case HAL_PIXEL_FORMAT_RAW10:
            numBytes = mRequestedConfig.width * mRequestedConfig.height * 10 / 8;
            break;
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            numBytes = mRequestedConfig.width * mRequestedConfig.height * 3 / 2;
            break;
        case HAL_PIXEL_FORMAT_RAW16:
            numBytes = mRequestedConfig.width * mRequestedConfig.height * 2;
            break;
        default:
            ALOGE("%s: Format %d not supported.", __FUNCTION__, mRequestedConfig.format);
            return -EINVAL;
    }

    mData.resize(numBytes, 0);
    mWidth = mRequestedConfig.width;
    mHeight = mRequestedConfig.height;
    mFormat = mRequestedConfig.format;

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
