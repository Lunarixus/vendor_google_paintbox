//#define LOG_NDEBUG 0
#define LOG_TAG "CaptureResultBlock"
#include <log/log.h>

#include <sys/sysinfo.h>
#include <system/graphics.h>

#include "CaptureResultBlock.h"
#include "HdrPlusPipeline.h"

namespace pbcamera {

static void logSysInfo() {
    struct sysinfo info;
    static unsigned long freeram = 0;
    int ret = sysinfo(&info);
    if (ret == 0) {
        ALOGD("HDR+ shot finished: freeram / totalram = %lu / %lu",
                info.freeram, info.totalram);
        if (freeram > info.freeram) {
            ALOGW("%lu bytes leaked in system memory!", freeram - info.freeram);
        }
        freeram = info.freeram;
    }
}

CaptureResultBlock::CaptureResultBlock(std::shared_ptr<MessengerToHdrPlusClient> messenger) :
        PipelineBlock("CaptureResultBlock"),
        mMessengerToClient(messenger) {
}

std::shared_ptr<CaptureResultBlock> CaptureResultBlock::newCaptureResultBlock(
        std::weak_ptr<HdrPlusPipeline> pipeline,
        std::shared_ptr<MessengerToHdrPlusClient> messenger) {
    auto block = std::shared_ptr<CaptureResultBlock>(new CaptureResultBlock(messenger));
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

CaptureResultBlock::~CaptureResultBlock() {
}

bool CaptureResultBlock::doWorkLocked() {
    ALOGV("%s", __FUNCTION__);

    // Check if we have any input.
    Input input = {};
    {
        std::unique_lock<std::mutex> lock(mQueueLock);
        if (mInputQueue.size() == 0) {
            // Nothing to do this time.
            ALOGV("%s: No input", __FUNCTION__);
            return false;
        }
        input = mInputQueue[0];
        mInputQueue.pop_front();
    }

    ALOGV("%s: Processing input", __FUNCTION__);

    // Copy over input data to result data.
    OutputResult blockResult = input;
    CaptureResult captureResult = {};
    captureResult.requestId = input.metadata.requestId;
    captureResult.metadata = *input.metadata.resultMetadata.get();

    for (auto buffer : blockResult.buffers) {
        StreamBuffer resultBuffer = {};

        // TODO(b/63809896): Locking data isn't necessary once it switches to ION buffers.
        buffer->lockData();

        std::shared_ptr<PipelineStream> stream = buffer->getStream().lock();
        if (stream == nullptr) {
            ALOGE("%s: Stream has been destroyed for request %d.", __FUNCTION__,
                    captureResult.requestId);
            // TODO: Send a failed capture result to client.
        } else {
            resultBuffer.streamId = stream->getStreamId();
            resultBuffer.dmaBufFd = buffer->getFd();
            if (resultBuffer.dmaBufFd == -1) {
                resultBuffer.data = buffer->getPlaneData(0);
            }
            resultBuffer.dataSize = buffer->getDataSize();
            captureResult.outputBuffers.push_back(resultBuffer);
        }
    }

    // Send the capture result to client.
    if (mMessengerToClient != nullptr) {
        mMessengerToClient->notifyCaptureResult(&captureResult);
    } else {
        ALOGE("%s: Messenger to client is null.", __FUNCTION__);
    }

    for (auto buffer : blockResult.buffers) {
        buffer->unlockData();
    }

    auto pipeline = mPipeline.lock();
    if (pipeline == nullptr) {
        ALOGE("%s: Pipeline is destroyed.", __FUNCTION__);
        return false;
    }

    pipeline->outputDone(blockResult);

    // Log available memory to detect memory leak.
    logSysInfo();

    return true;
}

status_t CaptureResultBlock::flushLocked() {
    // Do nothing because CaptureResultBlock doesn't keep any pending requets.
    return 0;
}

} // pbcamera
