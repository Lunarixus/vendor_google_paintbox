#ifndef PAINTBOX_HDR_PLUS_PIPELINE_SOURCE_CAPTURE_BLOCK_H
#define PAINTBOX_HDR_PLUS_PIPELINE_SOURCE_CAPTURE_BLOCK_H

#include "HdrPlusMessageTypes.h"
#include "MessengerToHdrPlusClient.h"
#include "PipelineBlock.h"
#include "PipelineBuffer.h"

namespace pbcamera {

class DequeueRequestThread;

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

    // Thread loop that dequeues completed buffers from capture service.
    void dequeueRequestThreadLoop();

private:
    // Use newSourceCaptureBlock to create a SourceCaptureBlock.
    SourceCaptureBlock(std::shared_ptr<MessengerToHdrPlusClient> messenger,
            std::unique_ptr<CaptureService> captureService);

    // Send a completed output result back to pipeline.
    void sendOutputResult(const OutputResult &result);

    // Abort an incomplete output request back to pipeline.
    void abortOutputRequest(const OutputRequest &request);

    // Handle completed capture for a pending request. This means the image buffer is captured and
    // it will wait for the metadata to arrive via notifyFrameMetadata.
    void handleCompletedCaptureForRequest(const OutputRequest &outputRequest,
            int64_t easelTimestamp);

    // Messenger for transferring the DMA buffer.
    std::shared_ptr<MessengerToHdrPlusClient> mMessengerToClient;

    // A queue of the pending output results that contain captured output buffers but do not
    // have corresponding frame metadata yet.
    std::mutex mPendingOutputResultQueueLock;
    std::deque<OutputResult> mPendingOutputResultQueue;

    // Capture service for MIPI capture.
    std::unique_ptr<CaptureService> mCaptureService;

    // A DequeueRequestThread to dequeue completed buffers from capture service.
    std::unique_ptr<DequeueRequestThread> mDequeueRequestThread;
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
    SourceCaptureBlock* mParent;

    // Protecting mPendingCaptureRequests and mExiting.
    std::mutex mDequeueThreadLock;
    std::deque<PipelineBlock::OutputRequest> mPendingCaptureRequests;
    bool mExiting;

    std::unique_ptr<std::thread> mDequeueRequestThread;
    std::condition_variable mEventCondition;
};

} // namespace pbcamera

#endif // PAINTBOX_HDR_PLUS_PIPELINE_SOURCE_CAPTURE_BLOCK_H
