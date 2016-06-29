#ifndef PAINTBOX_HDR_PLUS_PIPELINE_H
#define PAINTBOX_HDR_PLUS_PIPELINE_H

#include <mutex>

#include "blocks/PipelineBlock.h"
#include "PipelineStream.h"
#include "HdrPlusTypes.h"
#include "MessengerToHdrPlusClient.h"


namespace pbcamera {

/**
 * HdrPlusPipeline
 *
 * HdrPlusPipeline class manages the pipeline of a use case including streams, buffers, pipeline
 * blocks, and buffer routes.
 *
 * An HdrPlusPipeline manages a number of PipelineBlocks such as SourceCaptureBlock for capturing
 * source buffer for the pipeline, HdrPlusProcessingBlock for HDR+ processing, CaptureResultBlock
 * for sending capture results to HDR+ client.
 *
 * An HdrPlusPipeline can be configured for 1 input steram and 1 or more output streams. Each stream
 * will contain a number of buffers. When configure() is called, HdrPlusPipeline will create
 * streams, stream buffers, and routes for all streams. For example, the route of the input stream
 * may be: SourceCaptureBlock -> HdrPlusProcessingBlock, which means HdrPlusPipeline initially will
 * take a buffer out of the input stream and send the buffer to SourceCaptureBlock to capture a
 * frame to the buffer. After a frame is captured, the buffer will be returned to HdrPlusPipeline
 * and HdrPlusPipeline will send the buffer to its next block, HdrPlusProcessingBlock. After the
 * last block in the route, HdrPlusPipeline will return the buffer back to its stream.
 */
class HdrPlusPipeline : public std::enable_shared_from_this<pbcamera::HdrPlusPipeline> {
public:
    virtual ~HdrPlusPipeline();

    /*
     * Create a HdrPlusPipeline.
     *
     * messengerToClient is a MessengerToHdrPlusClient to send messages to HDR+ client.
     *
     * Returns a std::shared_ptr<HdrPlusPipeline> pointing to a HdrPlusPipeline on
     *         success.
     * Returns a std::shared_ptr<HdrPlusPipeline> pointing to nullptr if it failed.
     */
    static std::shared_ptr<HdrPlusPipeline> newPipeline(
            std::shared_ptr<MessengerToHdrPlusClient> messengerToClient);

    /*
     * Configure the pipeline with the specified streams, allocate buffers
     * for each stream, and create routes for all streams.
     *
     * inputConfig is input stream configuration.
     * outputConfigs is a vector of output stream configurations.
     *
     * Returns:
     *  0:              on success.
     *  -EINVAL:        if inputConfig or outputConfigs contains invalid values
     *                  or configurations that are not supported.
     *  -ENODEV:        if the pipeline cannot be configured due to a serious error.
     */
    status_t configure(const StreamConfiguration &inputConfig,
            const std::vector<StreamConfiguration> &outputConfigs);

    /*
     * Issue a capture request. HdrPlusPipeline will send the output buffers via
     * MessengerToHdrPlusClient
     *
     * Returns:
     *  0:              on success.
     *  -EINVAL:        if the request is invalid such as containing invalid stream IDs.
     */
    status_t submitCaptureRequest(const CaptureRequest &request);

    /*
     * Called by a PipelineBlock to send BlockInput back to HdrPlusPipeline after it's done
     * with the input buffers. HdrPlusPipeline will send the input buffers to the next block.
     * Note that "input" here means input to the block rather than the input streams of the
     * pipeline overall.
     */
    void inputDone(PipelineBlock::Input input);

    /*
     * Called by a PipelineBlock to send BlockOutputResult back to HdrPlusPipeline after the output
     * buffers are processed. HdrPlusPipeline will send the output buffers to the next block.
     * Note that "output" here means output to the block rather than the output streams of the
     * pipeline overall.
     */
    void outputDone(PipelineBlock::OutputResult outputResult);

    /*
     * Called by a PipelineBlock to abort BlockInput. HdrPlusPipeline will return the input buffers
     * back to their streams instead of sending them to the next block.
     * Note that "input" here means input to the block rather than the input streams of the
     * pipeline overall.
     */
    void inputAbort(PipelineBlock::Input input);

    /*
     * Called by a PipelineBlock to abort BlockOutputRequest. HdrPlusPipeline will return the
     * output buffers back to their streams instead of sending it to the next block.
     * Note that "output" here means output to the block rather than the output streams of the
     * pipeline overall.
     */
    void outputRequestAbort(PipelineBlock::OutputRequest outputRequest);

private:
    // Use newPipeline to create a HdrPlusPipeline.
    HdrPlusPipeline(std::shared_ptr<MessengerToHdrPlusClient> messengerToClient);

    // A stream route is a vector of pipeline blocks.
    typedef std::vector<std::shared_ptr<PipelineBlock>> StreamRoute;

    // Default number of buffers in input stream.
    const int kDefaultNumInputBuffers = 10;

    // Default number of buffers in output streams.
    const int kDefaultNumOutputBuffers = 3;

    // Time to wait when getting a buffer from a stream.
    const uint32_t kGetBufferTimeoutMs = 5000;

    // Time to wait for a block to stop.
    const uint32_t kStopBlockTimeoutMs = 5000;

    /*
     * The pipeline state.
     */
    enum PipelineState {
        // Pipeline is not configured yet. This is the initial state.
        STATE_UNCONFIGURED = 0,
        // Pipeline is running. This is the state after configure() return 0.
        STATE_RUNNING,
        // Pipeline is stopping. This is the state when pipeline initiates stopping process.
        STATE_STOPPING,
        // Pipeline is stopped. Pipeline blocks are also stopped.
        STATE_STOPPED,
    };

    // Create streams and buffers with mApiLock held.
    status_t createStreamsLocked(const StreamConfiguration &inputConfig,
            const std::vector<StreamConfiguration> &outputConfigs);

    // Create blocks and stream routes with mApiLock held.
    status_t createBlocksAndStreamRouteLocked();

    // Destroy streams, buffer, and routes with mApiLock held.
    void destroyLocked();

    // Return the next block in the route of a buffer with mApiLock held.
    std::shared_ptr<PipelineBlock> getNextBlockLocked(const PipelineBuffer &buffer);

    // Abort a BlockOutputRequest in progress and return the output buffers to their streams.
    void abortRequest(PipelineBlock::OutputRequest *outputRequest);

    // Start running the pipeline with mApiLock held.
    status_t startRunningPipelineLocked();

    // Stop the pipeline with mApiLock held.
    status_t stopPipelineLocked();

    // Return buffers to their streams.
    void returnBufferToStream(const PipelineBlock::PipelineBufferSet &buffers);

    // Mutex protecting public API methods.
    mutable std::mutex mApiLock;

    /*
     * Input stream that contains input buffers of the pipeline. Input stream buffers
     * hold frames captured from the sensor or HDR+ client.
     */
    std::shared_ptr<PipelineStream> mInputStream;

    /*
     * Output streams that contains output buffers of the pipeline. Output stream buffers hold
     * processed frames that will be sent to the client.
     */
    std::vector<std::shared_ptr<PipelineStream>> mOutputStreams;

    // Block to capture frames for input stream buffers.
    std::shared_ptr<PipelineBlock> mSourceCaptureBlock;
    // Block to fake process the input buffers to produce output buffers.
    std::shared_ptr<PipelineBlock> mDummyProcessingBlock;
    // Block to send capture results to client.
    std::shared_ptr<PipelineBlock> mCaptureResultBlock;

    // All created blocks.
    std::vector<std::shared_ptr<PipelineBlock>> mBlocks;

     // Map from PipelineStream to StreamRoute. Storing the stream routes.
    std::map<std::shared_ptr<PipelineStream>, StreamRoute> mStreamRoutes;
    // MessengerToHdrPlusClient to send messages to the client.
    std::shared_ptr<MessengerToHdrPlusClient> mMessengerToClient;

    // Pipeline state
    PipelineState mState;
};

} // namespace pbcamera

#endif // PAINTBOX_HDR_PLUS_PIPELINE_H