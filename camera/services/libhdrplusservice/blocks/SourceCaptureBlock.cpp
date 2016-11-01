//#define LOG_NDEBUG 0
#define LOG_TAG "SourceCaptureBlock"
#include "Log.h"

#include <inttypes.h>
#include <system/graphics.h>

#include "SourceCaptureBlock.h"
#include "HdrPlusPipeline.h"

namespace pbcamera {

SourceCaptureBlock::SourceCaptureBlock(std::shared_ptr<MessengerToHdrPlusClient> messenger) :
        PipelineBlock("SourceCaptureBlock"),
        mMessengerToClient(messenger) {
}

SourceCaptureBlock::~SourceCaptureBlock(){
}

std::shared_ptr<SourceCaptureBlock> SourceCaptureBlock::newSourceCaptureBlock(
        std::weak_ptr<HdrPlusPipeline> pipeline,
        std::shared_ptr<MessengerToHdrPlusClient> messenger) {
    auto block = std::shared_ptr<SourceCaptureBlock>(new SourceCaptureBlock(messenger));
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

    // For input buffers coming from the client via notifyDmaInputBuffer(), there is nothing to do
    // here.
    return false;
}

void SourceCaptureBlock::notifyDmaInputBuffer(const DmaImageBuffer &dmaInputBuffer,
        int64_t mockingEaselTimestampNs) {
    ALOGV("%s", __FUNCTION__);

    OutputRequest outputRequest;

    {
        std::unique_lock<std::mutex> lock(mQueueLock);
        if (mOutputRequestQueue.size() == 0) {
            ALOGE("%s: No output request available.. Dropping this input buffer.", __FUNCTION__);
            return;
        }

        outputRequest = mOutputRequestQueue[0];

        // Make sure there is only 1 output buffer in the request.
        if (outputRequest.buffers.size() != 1) {
            ALOGE("%s: The request has %d output buffers but only 1 output buffer is supported.",
                    __FUNCTION__, (int)outputRequest.buffers.size());
            mOutputRequestQueue.pop_front();
            abortOutputRequest(outputRequest);
            return;
        }

        auto stream = outputRequest.buffers[0]->getStream().lock();
        if (stream == nullptr) {
            ALOGE("%s: Buffer's stream is destroyed.", __FUNCTION__);
            return;
        }

        // Check if the stream id matches.
        if (static_cast<int>(dmaInputBuffer.streamId) != stream->getStreamId()) {
            ALOGE("%s: Got an input buffer for stream %d but the stream id should be %d.",
                    __FUNCTION__, dmaInputBuffer.streamId, stream->getStreamId());
            return;
        }

        mOutputRequestQueue.pop_front();
    }

    // Transfer the DMA buffer.
    status_t res = mMessengerToClient->transferDmaBuffer(dmaInputBuffer.dmaHandle,
            outputRequest.buffers[0]->getPlaneData(0), outputRequest.buffers[0]->getDataSize());
    if (res != 0) {
        ALOGE("%s: Transferring DMA buffer failed: %s (%d).", __FUNCTION__,
                strerror(-res), res);

        // Put the output request back to the queue.
        std::unique_lock<std::mutex> lock(mQueueLock);
        mOutputRequestQueue.push_front(outputRequest);
        return;
    }

    OutputResult result = {};
    result.buffers = outputRequest.buffers;
    result.route = outputRequest.route;
    result.metadata.frameMetadata = std::make_shared<FrameMetadata>();
    result.metadata.frameMetadata->easelTimestamp = mockingEaselTimestampNs;

    // Put the output result to pending queue waiting for the frame metadata to arrive.
    {
        std::unique_lock<std::mutex> lock(mPendingOutputResultQueueLock);
        mPendingOutputResultQueue.push_back(result);
    }

    // Notify the client of the Easel timestamp.
    // This is technically not needed because the mocking Easel timestamp cames from the client but
    // this models the behavior of the real use case where the timestamp comes from Easel.
    mMessengerToClient->notifyFrameEaselTimestampAsync(mockingEaselTimestampNs);
}

void SourceCaptureBlock::sendOutputResult(const OutputResult &result) {
    auto pipeline = mPipeline.lock();
    if (pipeline == nullptr) {
        ALOGE("%s: Pipeline is destroyed.", __FUNCTION__);
        return;
    }

    pipeline->outputDone(result);
}

void SourceCaptureBlock::abortOutputRequest(const OutputRequest &request) {
    auto pipeline = mPipeline.lock();
    if (pipeline == nullptr) {
        ALOGE("%s: Pipeline is destroyed.", __FUNCTION__);
        return;
    }

    pipeline->outputRequestAbort(request);
}

void SourceCaptureBlock::notifyFrameMetadata(const FrameMetadata &metadata) {
    ALOGV("%s: got frame metadata for timestamp %" PRId64, __FUNCTION__, metadata.easelTimestamp);

    std::unique_lock<std::mutex> lock(mPendingOutputResultQueueLock);
    // Look for a pending output result that has the same timestamp.
    auto result = mPendingOutputResultQueue.begin();
    while (result != mPendingOutputResultQueue.end()) {
        if (result->metadata.frameMetadata->easelTimestamp == metadata.easelTimestamp) {
            *result->metadata.frameMetadata = metadata;

            // Send out the completed output result.
            sendOutputResult(*result);
            mPendingOutputResultQueue.erase(result);
            return;
        }

        result++;
    }

    ALOGE("%s: Cannot find an output buffer with easel timestamp %" PRId64, __FUNCTION__,
            metadata.easelTimestamp);
}

} // pbcamera
