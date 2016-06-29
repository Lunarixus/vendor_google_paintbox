//#define LOG_NDEBUG 0
#define LOG_TAG "SourceCaptureBlock"
#include <utils/Log.h>

#include <system/graphics.h>

#include "SourceCaptureBlock.h"
#include "HdrPlusPipeline.h"

namespace pbcamera {

SourceCaptureBlock::SourceCaptureBlock() : PipelineBlock("SourceCaptureBlock") {
}

SourceCaptureBlock::~SourceCaptureBlock(){
}

std::shared_ptr<SourceCaptureBlock> SourceCaptureBlock::newSourceCaptureBlock(
        std::weak_ptr<HdrPlusPipeline> pipeline) {
    auto block = std::shared_ptr<SourceCaptureBlock>(new SourceCaptureBlock());
    if (block == nullptr) {
        ALOGE("%s: Failed to create a block instance.", __FUNCTION__);
        return nullptr;
    }

    status_t res = block->create(pipeline);
    if (res != 0) {
        ALOGE("%s: Failed to create block %s", __FUNCTION__, block->getName());
        return nullptr;
    }

    return block;
}

bool SourceCaptureBlock::doWorkLocked() {
    ALOGV("%s", __FUNCTION__);

    // Check if we have any output request.
    OutputRequest outputRequest = {};

    {
        std::unique_lock<std::mutex> lock(mQueueLock);
        if (mOutputRequestQueue.size() == 0) {
            // Nothing to do this time.
            ALOGV("%s: No output request.", __FUNCTION__);
            return false;
        }
        outputRequest = mOutputRequestQueue[0];
        mOutputRequestQueue.pop_front();
    }

    ALOGV("%s: Processing output request", __FUNCTION__);

    // TODO: capture the buffer from the client.
    OutputResult result = {};
    result.buffers = outputRequest.buffers;

    auto pipeline = mPipeline.lock();
    if (pipeline == nullptr) {
        ALOGE("%s: Pipeline is destroyed.", __FUNCTION__);
        return false;
    }

    // Send out output result.
    pipeline->outputDone(result);
    return true;
}

} // pbcamera
