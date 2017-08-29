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
     * Set the static metadata of current camera device.
     *
     * metadata is the static metadata.
     *
     * Returns:
     *  0:         on success.
     *  -EINVAL:   if metadata contains invalid values.
     */
    status_t setStaticMetadata(const StaticMetadata& metadata);

    /*
     * Configure the pipeline with the specified streams, allocate buffers
     * for each stream, and create routes for all streams.
     *
     * inputConfig is input configuration about sensor mode or input stream from AP.
     * outputConfigs is a vector of output stream configurations.
     *
     * Returns:
     *  0:              on success.
     *  -EINVAL:        if inputConfig or outputConfigs contains invalid values
     *                  or configurations that are not supported.
     *  -ENODEV:        if the pipeline cannot be configured due to a serious error.
     */
    status_t configure(const InputConfiguration &inputConfig,
            const std::vector<StreamConfiguration> &outputConfigs);

    /*
     * Enable or disable ZSL HDR+ mode.
     *
     * When ZSL HDR+ mode is enabled, Easel will capture ZSL RAW buffers. ZSL HDR+ mode should be
     * disabled to reduce power consumption when HDR+ processing is not necessary, e.g in video
     * mode.
     *
     * enabled is a flag indicating whether to enable ZSL HDR+ mode.
     *
     * Returns:
     *  0:          on success.
     *  -ENODEV:    if HDR+ service is not connected, or streams are not configured.
     */
    status_t setZslHdrPlusMode(bool enabled);

    /*
     * Submit a capture request. HdrPlusPipeline will send the output buffers via
     * MessengerToHdrPlusClient
     *
     * request is the capture request submitted.
     * metadata is the metadata for this request.
     *
     * Returns:
     *  0:              on success.
     *  -EINVAL:        if the request is invalid such as containing invalid stream IDs.
     */
    status_t submitCaptureRequest(const CaptureRequest &request, const RequestMetadata &metadata);

    /*
     * Notify the pipeline of a DMA input buffer. The DMA image buffer will be transferred after
     * this method returns successfully.
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

    // Default number of buffers in input stream.
    const int kDefaultNumInputBuffers = 11;

    // Default number of buffers in output streams.
    const int kDefaultNumOutputBuffers = 1;

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
        // Pipeline is stopped. Pipeline blocks are also stopped. This is the state when
        // stream is configured and ZSL HDR+ mode is disabled.
        STATE_STOPPED,
        // Pipeline is running. This is the state when ZSL HDR+ mode is enabled.
        STATE_RUNNING,
        // Pipeline is stopping. This is a transient state when pipeline initiates stopping process.
        STATE_STOPPING,
    };

    // Create streams and buffers with mApiLock held.
    status_t createStreamsLocked(const InputConfiguration &inputConfig,
            const std::vector<StreamConfiguration> &outputConfigs);

    // Create blocks and stream routes with mApiLock held.
    status_t createBlocksAndStreamRouteLocked(const SensorMode *sensorMode);

    // Destroy streams, buffer, and routes with mApiLock held.
    void destroyLocked();

    // Return the next block in the route of block data with mApiLock held.
    std::shared_ptr<PipelineBlock> getNextBlockLocked(const PipelineBlock::BlockIoData &blockData);

    // Abort a BlockOutputRequest in progress and return the output buffers to their streams.
    void abortRequest(PipelineBlock::OutputRequest *outputRequest);

    // Start running the pipeline with mApiLock held.
    status_t startRunningPipelineLocked();

    // Stop the pipeline with mApiLock held.
    status_t stopPipelineLocked();

    // Return buffers to their streams.
    void returnBufferToStream(const PipelineBlock::PipelineBufferSet &buffers);

    // Abort block IO data. If it has a circular route, queue it to the first block. Otherwise,
    // return its buffers to their streams.
    void abortBlockIoData(PipelineBlock::BlockIoData *data);

    // Mutex protecting public API methods.
    mutable std::mutex mApiLock;

    // Static metadata of current camera device.
    std::shared_ptr<StaticMetadata> mStaticMetadata;

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
    // Block to do HDR+ processing on the input buffers to produce output buffers.
    std::shared_ptr<PipelineBlock> mHdrPlusProcessingBlock;
    // Block to send capture results to client.
    std::shared_ptr<PipelineBlock> mCaptureResultBlock;

    // Route for input stream.
    PipelineBlock::BlockIoDataRoute mInputStreamRoute;
    // Route for output streams.
    PipelineBlock::BlockIoDataRoute mOutputStreamRoute;

    // All created blocks.
    std::vector<std::shared_ptr<PipelineBlock>> mBlocks;

    // MessengerToHdrPlusClient to send messages to the client.
    std::shared_ptr<MessengerToHdrPlusClient> mMessengerToClient;

    // Pipeline state
    PipelineState mState;

    // IMX memory allocate handle to allocate IMX buffers.
    ImxMemoryAllocatorHandle mImxMemoryAllocatorHandle;
};

} // namespace pbcamera

#endif // PAINTBOX_HDR_PLUS_PIPELINE_H
