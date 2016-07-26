//#define LOG_NDEBUG 0
#define LOG_TAG "DummyProcessingBlock"
#include <utils/Log.h>

#include <system/graphics.h>

#include "DummyProcessingBlock.h"
#include "HdrPlusPipeline.h"

namespace pbcamera {

DummyProcessingBlock::DummyProcessingBlock() :
        PipelineBlock("DummyProcessingBlock") {
}

DummyProcessingBlock::~DummyProcessingBlock() {
}

std::shared_ptr<DummyProcessingBlock> DummyProcessingBlock::newDummyProcessingBlock(
        std::weak_ptr<HdrPlusPipeline> pipeline) {
    auto block = std::shared_ptr<DummyProcessingBlock>(new DummyProcessingBlock());
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

bool DummyProcessingBlock::doWorkLocked() {
    ALOGV("%s", __FUNCTION__);

    Input input = {};
    OutputRequest outputRequest = {};

    // Check if we have any input and output request.
    {
        std::unique_lock<std::mutex> lock(mQueueLock);
        if (mInputQueue.size() == 0) {
            // Nothing to do this time.
            ALOGV("%s: No input", __FUNCTION__);
            return false;
        } else if (mOutputRequestQueue.size() == 0) {
            // Nothing to do this time.
            ALOGV("%s: No output request", __FUNCTION__);
            return false;
        }

        input = mInputQueue[0];
        mInputQueue.pop_front();

        outputRequest = mOutputRequestQueue[0];
        mOutputRequestQueue.pop_front();
    }

    // Set all data bytes to a constant value.
    OutputResult outputResult = {};
    for (auto outputBuffer : outputRequest.buffers) {
        uint8_t *data = outputBuffer->getData();
        memset(data, kSetByteValue, outputBuffer->getDataSize());
        outputResult.buffers.push_back(outputBuffer);
    }

    // Update frame metadata with input's frame metadata.
    outputResult.metadata.frameMetadata = input.metadata.frameMetadata;

    // Update request ID with output's request ID.
    outputResult.metadata.requestId = outputRequest.metadata.requestId;

    auto pipeline = mPipeline.lock();
    if (pipeline == nullptr) {
        ALOGE("%s: Pipeline is destroyed.", __FUNCTION__);
        return false;
    }

    pipeline->outputDone(outputResult);
    pipeline->inputDone(input);

    return true;
}

} // pbcamera
