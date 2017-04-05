//#define LOG_NDEBUG 0
#define LOG_TAG "PipelineBlock"
#include <utils/Log.h>

#include "PipelineBlock.h"
#include "HdrPlusPipeline.h"

namespace pbcamera {

PipelineBlock::PipelineBlock(const char *blockName, int32_t eventTimeoutMs) :
        mName(blockName),
        mState(STATE_INVALID),
        mEventCounts(0),
        mEventTimeoutMs(eventTimeoutMs) {
}

PipelineBlock::~PipelineBlock() {
    destroy();
}

static void threadLoopFunc(PipelineBlock *block) {
    if (block == nullptr) return;

    block->threadLoop();
}

int PipelineBlock::create(std::weak_ptr<HdrPlusPipeline> pipeline) {
    ALOGV("Creating block: %s", mName.data());

    std::unique_lock<std::mutex> lock(mApiLock);
    if (mState != STATE_INVALID) return -EEXIST;

    mPipeline = pipeline;
    mThread = std::unique_ptr<std::thread>(new std::thread(threadLoopFunc, this));
    mState = STATE_STOPPED;

    return 0;
}

void PipelineBlock::destroy() {
    std::unique_lock<std::mutex> lock(mApiLock);

    // Stop the worker thread
    if (mThread != nullptr) {
        {
            std::unique_lock<std::mutex> lock(mWorkLock);
            mState = STATE_SHUTTING_DOWN;
        }

        notifyWorkerThreadEvent();
        mThread->join();
        mThread = nullptr;
    }
    {
        std::unique_lock<std::mutex> lock(mWorkLock);
        mState = STATE_INVALID;
    }
}

status_t PipelineBlock::run() {
    std::unique_lock<std::mutex> lock(mApiLock);
    switch (mState) {
        case STATE_RUNNING:
            return 0;
        case STATE_STOPPED:
        {
            {
                // Notify the worker thread.
                std::unique_lock<std::mutex> lock(mWorkLock);
                mState = STATE_RUNNING;
            }

            notifyWorkerThreadEvent();
            return 0;
        }
        case STATE_INVALID:
        case STATE_STOPPING:
        case STATE_SHUTTING_DOWN:
        default:
            ALOGE("%s: Block cannot run from state: %d", __FUNCTION__, mState);
            return -EINVAL;
    }
}

status_t PipelineBlock::stopAndFlush(uint32_t timeoutMs) {
    ALOGV("Block(%s) %s.", mName.data(), __FUNCTION__);
    std::unique_lock<std::mutex> lock(mApiLock);
    if (mState != STATE_RUNNING) return 0;

    {
        // Stopping the worker thread.
        std::unique_lock<std::mutex> workLock(mWorkLock);
        mState = STATE_STOPPING;
    }

    notifyWorkerThreadEvent();

    {
        // Wait until the state becomes STOPPED.
        std::unique_lock<std::mutex> workLock(mWorkLock);
        if (mStoppedCondition.wait_for(workLock, std::chrono::milliseconds(timeoutMs),
                [&] { return mState == STATE_STOPPED; }) == false) {
            return -ETIMEDOUT;
        }
    }

    // Return all pending inputs and output requests.
    {
        auto pipeline = mPipeline.lock();
        if (pipeline != nullptr) {
            std::unique_lock<std::mutex> queueLock(mQueueLock);
            for (auto input : mInputQueue) {
                pipeline->inputAbort(input);
            }
            for (auto output : mOutputRequestQueue) {
                pipeline->outputRequestAbort(output);
            }
        }

        mInputQueue.clear();
        mOutputRequestQueue.clear();
    }

    return 0;
}

void PipelineBlock::handleTimeoutLocked() {
    ALOGI("%s: Block %s waiting for events timed out after %d ms.", __FUNCTION__, mName.data(),
        mEventTimeoutMs);
}

void PipelineBlock::threadLoop() {
    ALOGV("Block(%s) %s.", mName.data(), __FUNCTION__);
    while (1) {
        {
            std::unique_lock<std::mutex> lock(mWorkLock);

            // Check the block state
            if (mState == STATE_STOPPING) {
                ALOGV("%s: Flushing block %s", __FUNCTION__, mName.data());
                flushLocked();
                ALOGV("%s: %s block thread stopped doing work.", __FUNCTION__, mName.data());
                mState = STATE_STOPPED;
                // Notify that worker thread has stopping doing work.
                mStoppedCondition.notify_one();
            } else if (mState == STATE_SHUTTING_DOWN) {
                ALOGV("%s: %s block thread exists.", __FUNCTION__, mName.data());
                return;
            }

            // Do block work if state is running.
            bool moreWork = true;
            while (moreWork && mState == STATE_RUNNING) {
                moreWork = doWorkLocked();
            }
        }

        // Wait for next event like new input or output request.
        bool gotEvent = true;
        {
            std::unique_lock<std::mutex> eventLock(mEventLock);
            if (mEventTimeoutMs == NO_EVENT_TIMEOUT) {
                mEventCondition.wait(eventLock, [&] { return mEventCounts > 0; });
            } else {
                gotEvent = mEventCondition.wait_for(eventLock,
                        std::chrono::milliseconds(mEventTimeoutMs),
                        [&] { return mEventCounts > 0; });
            }

            if (gotEvent) {
                mEventCounts--;
            }
        }

        if (!gotEvent) {
            std::unique_lock<std::mutex> lock(mWorkLock);
            handleTimeoutLocked();
        }
    }
}

status_t PipelineBlock::queueInput(Input *input) {
    ALOGV("%s: %s", mName.data(), __FUNCTION__);
    if (input == nullptr) return -EINVAL;

    // Increment current block index.
    input->route.currentBlockIndex++;

    // Queue input and notify worker thread.
    {
        std::unique_lock<std::mutex> lock(mQueueLock);
        mInputQueue.push_back(*input);
    }

    notifyWorkerThreadEvent();

    return 0;
}

status_t PipelineBlock::queueOutputRequest(OutputRequest *outputRequest) {
    ALOGV("%s: %s", mName.data(), __FUNCTION__);
    if (outputRequest == nullptr) return -EINVAL;

    // Increment current block index.
    outputRequest->route.currentBlockIndex++;;

    // Queue output request and notify worker thread.
    {
        std::unique_lock<std::mutex> lock(mQueueLock);
        mOutputRequestQueue.push_back(*outputRequest);
    }

    notifyWorkerThreadEvent();

    return 0;
}

void PipelineBlock::notifyWorkerThreadEvent() {
    std::unique_lock<std::mutex> eventLock(mEventLock);
    mEventCounts++;
    mEventCondition.notify_one();
}

const char* PipelineBlock::getName() const {
    return mName.data();
}

} // namespace pbcamera
