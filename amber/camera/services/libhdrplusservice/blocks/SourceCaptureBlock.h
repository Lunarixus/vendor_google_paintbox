#ifndef PAINTBOX_HDR_PLUS_PIPELINE_SOURCE_CAPTURE_BLOCK_H
#define PAINTBOX_HDR_PLUS_PIPELINE_SOURCE_CAPTURE_BLOCK_H

#include <easelcontrol.h>

#include "HdrPlusMessageTypes.h"
#include "MessengerToHdrPlusClient.h"
#include "PipelineBlock.h"
#include "PipelineBuffer.h"

namespace pbcamera {

class DequeueRequestThread;
class TimestampNotificationThread;

/*
 * SourceCaptureBlock is a pipeline block that captures frames from MIPI (or the client for testing
 * purpose) to buffers in PipelineBlock::OutputRequest. SourceCaptureBlock's doWorkLocked() starts
 * its work when a PipelineBlock::OutputRequest is available. PipelineBlock::Input is ignored in
 * SourceCaptureBlock.
 */
class SourceCaptureBlock : public PipelineBlock {
    // DequeueRequestThread is a friend class so it can access mCaptureService.
    friend class DequeueRequestThread;
public:
    virtual ~SourceCaptureBlock();

    /*
     * Create a SourceCaptureBlock.
     *
     * pipeline is the pipeline this block belongs to.
     * messenger is a messenger to send messages to HDR+ client.
     * sensorMode is the camera sensor mode information. If nullptr, the input images comes
     *               from AP via notifyDmaInputBuffer().
     *
     * Returns a std::shared_ptr<SourceCaptureBlock> pointing to a SourceCaptureBlock on
     *         success.
     * Returns a std::shared_ptr<SourceCaptureBlock> pointing to nullptr if it failed.
     */
    static std::shared_ptr<SourceCaptureBlock> newSourceCaptureBlock(
            std::weak_ptr<HdrPlusPipeline> pipeline,
            std::shared_ptr<MessengerToHdrPlusClient> messenger,
            const SensorMode *sensorMode);

    /*
     * Notify about a DMA input buffer. SourceCaptureBlock will use the DMA image buffer as an
     * input to produce output requests.
     *
     * dmaInputBuffer is the DMA input buffer to be transferred.
     * mockingEaselTimestampNs is the mocking Easel timestamp of the input buffer.
     */
    void notifyDmaInputBuffer(const DmaImageBuffer &dmaInputBuffer,
            int64_t mockingEaselTimestampNs);

    /*
     * Notify the pipeline of a frame metadata.
     *
     * metadata is the metadata of a frame that AP captured.
     */
    void notifyFrameMetadata(const FrameMetadata &metadata);

    // Override PipelineBlock::doWorkLocked.
    bool doWorkLocked() override;

    // Override PipelineBlock::flushLocked.
    status_t flushLocked() override;

    // Override PipelineBlock::handleTimeoutLocked.
    void handleTimeoutLocked() override;

    // Notify IPU processing is going to start. If continuousCapturing is true, Source Capture
    // block will continue capturing. Otherwise, it will stop capturing.
    void notifyIpuProcessingStart(bool continuousCapturing);

    // Notify IPU processing is done.
    void notifyIpuProcessingDone();

    // Pause capturing.
    void pauseCapture();

private:
    // Timeout duration for waiting for events.
    static const int32_t BLOCK_EVENT_TIMEOUT_MS = 500;

    // Timeout duration for waiting for frame metadata. If a pending output result has an Easel
    // timestamp that's older than this value, AP may have dropped a frame or Easel timestamp is
    // not accurate.
    static const int64_t FRAME_METADATA_TIMEOUT_NS = 500000000; // 500ms

    // Number of frames to capture to enter a stable state to change clock mode.
    static const int32_t kStableFrameCount = 30;

    static const int32_t kInvalidFrameCounterId = -1;

    // Use newSourceCaptureBlock to create a SourceCaptureBlock.
    SourceCaptureBlock(std::shared_ptr<MessengerToHdrPlusClient> messenger,
        const paintbox::CaptureConfig &config);

    // Create capture service. Protected by mSourceCaptureLock.
    status_t createCaptureServiceLocked();

    // Destroy capture service. Protected by mSourceCaptureLock.
    void destroyCaptureServiceLocked();

    // Send a completed output result back to pipeline.
    void sendOutputResult(const OutputResult &result);

    // Abort an incomplete output request back to pipeline.
    void abortOutputRequest(const OutputRequest &request);

    // Handle completed capture for a pending request. This means the image buffer is captured and
    // it will wait for the metadata to arrive via notifyFrameMetadata.
    void handleCompletedCaptureForRequest(const OutputRequest &outputRequest,
            int64_t easelTimestamp);

    // Remove any staled pending output result.
    void removeTimedoutPendingOutputResult();

    // Request a capture to prevent possible frame drops.
    void requestCaptureToPreventFrameDrop();

    // DMA transfer a buffer.
    status_t transferDmaBuffer(const DmaImageBuffer &dmaInputBuffer, PipelineBuffer *buffer);

    // Invoked when mDequeueRequestThread has captured some amount of frames after
    // requestFrameCounterNotification() is called.
    // frameCounterId is the frame counter ID passed to requestFrameCounterNotification().
    void notifyFrameCounterDone(int32_t frameCounterId);

    // Pause and resume capture service. Must be protected by mSourceCaptureLock.
    void pauseCaptureServiceLocked();
    void resumeCaptureServiceLocked(bool startFrameCounter, int32_t frameCounterId);

    // Change clock mode to capture. Must be protected by mSourceCaptureLock.
    void changeToCaptureClockModeLocked();

    // Messenger for transferring the DMA buffer.
    std::shared_ptr<MessengerToHdrPlusClient> mMessengerToClient;

    // A queue of the pending output results that contain captured output buffers but do not
    // have corresponding frame metadata yet.
    std::mutex mPendingOutputResultQueueLock;
    std::deque<OutputResult> mPendingOutputResultQueue;

    // Whether to capture input buffers from MIPI or from AP.
    bool mIsMipiInput;

    // Capture service for MIPI capture.
    std::mutex mCaptureServiceLock;
    std::unique_ptr<paintbox::CaptureService> mCaptureService; // Protected by mCaptureServiceLock.

    // Capture config used to create the capture service.
    const paintbox::CaptureConfig mCaptureConfig;

    // A DequeueRequestThread to dequeue completed buffers from capture service.
    // Protected by mCaptureServiceLock.
    std::unique_ptr<DequeueRequestThread> mDequeueRequestThread;

    // A thread to notify AP about Easel timestmap.
    std::unique_ptr<TimestampNotificationThread> mTimestampNotificationThread;

    std::mutex mSourceCaptureLock;
    bool mCaptureServicePaused;  // If capture service is paused. Protected by mSourceCaptureLock.
    // Current clock mode. Protected by mSourceCaptureLock.
    EaselControlServer::ClockMode mClockMode;

    std::mutex mFrameCounterLock;
    // Last request frame counter ID that is expected.
    int32_t mLastRequestedFrameCounterId;
    // Last frame counter ID received from dequeue request thread. Protected by mFrameCounterLock.
    int32_t mLastFinishedFrameCounterId;
};

// DequeueRequestThread dequeues completed buffers from capture service.
class DequeueRequestThread {
public:
    DequeueRequestThread(SourceCaptureBlock* parent);
    virtual ~DequeueRequestThread();

    // Add a pending request. If there is a pending request, DequeueRequestThread will wait
    // on a completed buffer from capture service.
    void addPendingRequest(PipelineBlock::OutputRequest request);

    // Start a frame counter. After frameCount frames have been captured, invoke
    // notifyFrameCounterDone.
    void requestFrameCounterNotification(uint32_t frameCount, int32_t requestId);

    // Thread loop that dequeues completed buffers from capture service.
    void dequeueRequestThreadLoop();

    // Signal the thread to exit.
    void signalExit();

    // Pause dequeue request thread.
    void pause();

    // Resume dequeue request thread.
    void resume();

private:
    const static int64_t kNsPerMs = 1000000;

    // Capture service needs at least 2 requests at all time to prevent frame drops. We need to
    // have at least 3 pending requests so when capture service is done with 1, it still has 2.
    const static uint32_t kMinNumPendingRequests = 3;

    // Check the number of pending requests and request more if needed to prevent frame drops.
    void checkNumberPendingRequests();

    SourceCaptureBlock* mParent;

    // Protecting mPendingCaptureRequests and mExiting.
    std::mutex mDequeueThreadLock;
    std::deque<PipelineBlock::OutputRequest> mPendingCaptureRequests;

    std::unique_ptr<std::thread> mThread;
    std::condition_variable mEventCondition;
    bool mFirstCaptureDone;

    // Frame counter to invoke notifyFrameCounterDone() when becoming 0 from 1.
    int32_t mFrameCounter;
    int32_t mFrameCounterId;

    // States of the thread.
    enum State {
        // Pausing the thread is requested.
        STATE_PAUSING = 0,
        // The thread is paused.
        STATE_PAUSED,
        // Resuming the thread is requested.
        STATE_RESUMING,
        // Thread is running.
        STATE_RUNNING,
        // Exiting the thread is requested.
        STATE_EXITING,
    };

    State mState; // State of the thread. Protected by mDequeueThreadLock.
    std::condition_variable mStateChangedCondition; // Used to signal the state change.
};

// TimestampNotificationThread creates a thread to send Easel timestamps to AP.
class TimestampNotificationThread {
public:
    TimestampNotificationThread(std::shared_ptr<MessengerToHdrPlusClient> messengerToClient);
    virtual ~TimestampNotificationThread();

    // Notify a new Easel timestamp asynchronously.
    void notifyNewEaselTimestampNs(int64_t easelTimestampNs);

    // Thread loop that sends Easel timestamps to AP.
    void threadLoop();

    // Signal the thread to exit.
    void signalExit();

private:
    std::unique_ptr<std::thread> mThread;
    std::shared_ptr<MessengerToHdrPlusClient> mMessengerToClient;

    std::mutex mEventLock;
    std::condition_variable mEventCondition;

    // The following variables must be protected by mEventLock.
    bool mExiting; // If requested to exit.
    std::deque<int64_t> mEaselTimestamps; // A queue of Easel timestamps to send to AP.
};

} // namespace pbcamera

#endif // PAINTBOX_HDR_PLUS_PIPELINE_SOURCE_CAPTURE_BLOCK_H
