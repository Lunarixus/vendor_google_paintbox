#ifndef PAINTBOX_HDR_PLUS_PIPELINE_SOURCE_CAPTURE_BLOCK_H
#define PAINTBOX_HDR_PLUS_PIPELINE_SOURCE_CAPTURE_BLOCK_H

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

    // Paused and resume capturing from MIPI. This is to work around the limitation that MIPI
    // capture and IPU HDR+ processing cannot happen at the same time. b/34854987
    void pause();
    void resume();

private:
    // Timeout duration for waiting for events.
    static const int32_t BLOCK_EVENT_TIMEOUT_MS = 500;

    // Timeout duration for waiting for frame metadata. If a pending output result has an Easel
    // timestamp that's older than this value, AP may have dropped a frame or Easel timestamp is
    // not accurate.
    static const int64_t FRAME_METADATA_TIMEOUT_NS = 500000000; // 500ms

    // Use newSourceCaptureBlock to create a SourceCaptureBlock.
    SourceCaptureBlock(std::shared_ptr<MessengerToHdrPlusClient> messenger,
        const CaptureConfig &config);

    // Create capture service.
    status_t createCaptureService();

    // Destroy capture service.
    void destroyCaptureService();

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

    // Messenger for transferring the DMA buffer.
    std::shared_ptr<MessengerToHdrPlusClient> mMessengerToClient;

    // A queue of the pending output results that contain captured output buffers but do not
    // have corresponding frame metadata yet.
    std::mutex mPendingOutputResultQueueLock;
    std::deque<OutputResult> mPendingOutputResultQueue;

    // Whether to capture input buffers from MIPI or from AP.
    bool mIsMipiInput;

    // Capture service for MIPI capture.
    std::unique_ptr<CaptureService> mCaptureService;

    // Capture config used to create the capture service.
    const CaptureConfig mCaptureConfig;

    // A DequeueRequestThread to dequeue completed buffers from capture service.
    std::unique_ptr<DequeueRequestThread> mDequeueRequestThread;

    // A thread to notify AP about Easel timestmap.
    std::unique_ptr<TimestampNotificationThread> mTimestampNotificationThread;

    // Whether to pause capturing from MIPI to work around b/34854987.
    std::mutex mPauseLock;
    bool mPaused;
};

// DequeueRequestThread dequeues completed buffers from capture service.
class DequeueRequestThread {
public:
    DequeueRequestThread(SourceCaptureBlock* parent);
    virtual ~DequeueRequestThread();

    // Add a pending request. If there is a pending request, DequeueRequestThread will wait
    // on a completed buffer from capture service.
    void addPendingRequest(PipelineBlock::OutputRequest request);

    // Thread loop that dequeues completed buffers from capture service.
    void dequeueRequestThreadLoop();

    // Signal the thread to exit.
    void signalExit();

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
    bool mExiting;

    std::unique_ptr<std::thread> mThread;
    std::condition_variable mEventCondition;
    bool mFirstCaptureDone;
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
