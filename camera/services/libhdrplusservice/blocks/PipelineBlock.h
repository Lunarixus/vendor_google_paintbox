#ifndef PAINTBOX_HDR_PLUS_PIPELINE_BLOCK_H
#define PAINTBOX_HDR_PLUS_PIPELINE_BLOCK_H

#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "PipelineBuffer.h"

namespace pbcamera {

class HdrPlusPipeline;

/**
 * PipelineBlock
 *
 * A PipelineBlock is a block that performs a specific (and usually lower level) task and produces
 * output results (PipelineBlock::OutputResult) given inputs (PipelineBlock::Input) and output
 * requests (PipelineBlock::OutputRequest). A sequence of blocks form a processing pipeline
 * that performs a higher level task. For example, a pipeline performing HDR+ processing with frames
 * from the sensor can have low level blocks including "capturing RAW frames from MIPI", "HDR+
 * processing", and "sending HDR+ processed frames to client."
 *
 * A PipelineBlock may not need both Input and OutputRequest to perform its task. For example,
 * SourceCaptureBlock doesn't need Input because it captures frames from MIPI to produce
 * OutputResult. CaptureResultBlock doesn't need OutputRequest because it sends CaptureResults
 * compiled with Input to the client.
 *
 * Note that in the context of a PipelineBlock, input means the input of this particular block and
 * output means the output of this particular block. One PipelineStream may be input of one block
 * and output of another block.
 */
class PipelineBlock : public std::enable_shared_from_this<PipelineBlock> {
public:
    virtual ~PipelineBlock();

    // A vector of buffers.
    typedef std::vector<PipelineBuffer*> PipelineBufferSet;

    // Block I/O data used when sending inputs and outputs between a pipeline and a block.
    struct BlockIoData {
        PipelineBufferSet buffers;
        // TODO: add metadata.
    };

    // Block input.
    typedef BlockIoData Input;
    // Block output request.
    typedef BlockIoData OutputRequest;
    // Block output result.
    typedef BlockIoData OutputResult;

    /*
     * Start running the block.
     *
     * Return:
     *  0:              if the block starts running and is ready to process input and output
     *                  requests when they are available.
     *  -EINVAL:        if the block is not in a valid state or create() is not called yet.
     */
    status_t run();

    /*
     * Stop the block and flush all pending inputs and output requests.
     *
     * timeoutMs is the amount of time to wait for the block to stop and flush.
     *
     * Returns:
     *  0:              if the block was stopped and returned all pending inputs and output
     *                  requests.
     *  -ETIMEDOUT:     if the block cannot be stopped within the specified amount of time.
     */
    status_t stopAndFlush(uint32_t timeoutMs);

    /*
     * Queue input data to the block. This is needed for blocks that require input data to perform
     * its task. This is not needed for blocks that do not need input data to perform its task, such
     * as SourceCaptureBlock, which captures frames from MIPI or the client.
     *
     * input contains input data.
     *
     * Returns:
     *  0:              on success.
     *  -EINVAL:        if input is null.
     */
    status_t queueInput(Input *input);

    /*
     * Queue an output request to the block. This is needed for blocks that require input data to
     * process output requests. This is not needed for blocks that don't require output requests to
     * perform its task, such as CaptureResultBlock, which sends the input buffers to the client.
     *
     * outputRequest is an output request. It can have more than 1 output buffer. All buffers in the
     *               request must be processed using the same source and parameters.
     *
     * Returns:
     *  0:              on success.
     *  -EINVAL:        if outputRequest is null.
     */
    status_t queueOutputRequest(OutputRequest *outputRequest);

    // Return a string of block name.
    const char *getName() const;

    // Thread loop for the worker thread.
    void threadLoop();

protected:
    /*
     * PipelineBlock is derived from std::enable_shared_from_this<PipelineBlock> so constructor
     * cannot be used to create an instance. Blocks derived from PipelineBlock should provide a
     * static method to return a std::shared_ptr<>. See SourceCaptureBlock::newSourceCaptureBlock()
     * for example.
     */
    PipelineBlock(const char *blockName);

    /*
     * Create the resources to run the block. Blocks derived from PipelineBlock should call this
     * method before returning a std::shared_ptr<> so the block is ready to run.
     *
     * pipeline is the HdrPlusPipeline this block belongs to.
     *
     * Returns:
     *  0:          if the block is created successfully.
     *  -EEXIST:    if the block is already created.
     */
    status_t create(std::weak_ptr<HdrPlusPipeline> pipeline);

    /*
     * Perform block task
     *
     * This will be called when the block receives an input or an output request. It should check
     * if it's ready to perform the task. Readiness depends on each block's task. For example,
     * SourceCaptureBlock does not require an input to capture frames from MIPI so
     * it will be ready as soon as it has an output request. MeteringBlock doesn't require an
     * output request so it will be ready as soon as it gets an input.
     *
     * After the block performs its task and produces outputs, it should call inputDone() and
     * outputRequestDone() to send the buffers and metadata to next block. If the block does
     * inplace processing and receives no output requests, it should only call outputDone() with
     * the inplace processed buffer and metadata.
     *
     * This will be called with mWorkLock held.
     *
     * Returns:
     *  true:           if there are more tasks to do and it will be called immediately.
     *  false:          if there is not enough data to perform the task. For example, input queue is
     *                  empty.
     */
    virtual bool doWorkLocked() = 0;

    /*
     * Protect mInputQueue and mOutputRequestQueue. A block must acquire this lock before accessing
     * mInputQueue or mOutputRequestQueue.
     */
    std::mutex mQueueLock;
    std::deque<Input> mInputQueue;
    std::deque<OutputRequest> mOutputRequestQueue;

    // Name of the block.
    std::string mName;

    // Pipeline that the block belongs to.
    std::weak_ptr<HdrPlusPipeline> mPipeline;

private:
    // States of the block.
    enum BlockState {
        // Block is invalid because its resources are not created yet. This is the initial state.
        STATE_INVALID = 0,
        // Block is created and stopped. The state after create() returns 0 or stopAndFlush().
        STATE_STOPPED,
        /*
         * Block is running and ready to perform the task when needed input and/or output are
         * avaiable. The state after run() returns 0.
         */
        STATE_RUNNING,
        // Block is being stopped.
        STATE_STOPPING,
        // Block is shutting down. Block thread should return to terminate itself.
        STATE_SHUTTING_DOWN,
    };

    // Destroy the resources of the block.
    void destroy();


    // State of the block.
    BlockState mState;

    // Protect public methods.
    std::mutex mApiLock;

    // A condition for stopping the block.
    std::condition_variable mStoppedCondition;

    /*
     * A counter of events like inputs or output requests. When larger than 0,
     * worker thread will not wait on mEventCondition. Must hold mEventLock to access.
     */
    int mEventCounts;

    // Protect mEventCounts from being accessed simultaneously.
    std::mutex mEventLock;

    // A condition that worker thread waits on if mEventCounts is 0.
    std::condition_variable mEventCondition;

    // Held when block is doing work.
    std::mutex mWorkLock;

    // Worker thread.
    std::unique_ptr<std::thread> mThread;
};

} // namespace pbcamera

#endif // PAINTBOX_HDR_PLUS_PIPELINE_BLOCK_H
