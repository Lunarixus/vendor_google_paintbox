#ifndef PAINTBOX_HDR_PLUS_PIPELINE_STREAM_H
#define PAINTBOX_HDR_PLUS_PIPELINE_STREAM_H

#include <deque>
#include <memory>

#include "blocks/PipelineBlock.h"
#include "PipelineBuffer.h"
#include "HdrPlusTypes.h"

namespace pbcamera {

/**
 * PipelineStream
 *
 * A PipelineStream is a stream in a pipeline and has a number of buffers with the same
 * configuration.
 */
class PipelineStream : public std::enable_shared_from_this<PipelineStream> {
public:
    virtual ~PipelineStream();

    /*
     * Create a PipelineStream.
     *
     * config is the configuration to create the stream of.
     * numBuffers is the number of buffers to create for the stream.
     *
     * Returns a std::shared_ptr<PipelineStream> pointing to a PipelineStream on success.
     * Returns a std::shared_ptr<PipelineStream> pointing to nullptr if it failed.
     */
    static std::shared_ptr<PipelineStream> newPipelineStream(const StreamConfiguration &config,
            int numBuffers);

    // Return whether the stream has the specified configuration.
    bool hasConfig(const StreamConfiguration &config) const;

    // Return the ID of the stream.
    int getStreamId() const;

    /**
     * Get a buffer from the stream that's available to use. If there is no buffer available, it
     * will wait until one becomes available or a specified amount of time has elapsed. If it
     * returns 0, the caller can access the buffer exclusively. It is the caller's responsibility
     * to call returnBuffer() to return the buffer to the stream when it no longer needs to access
     * the buffer but the stream still has the ownership. If the stream is destroyed, all its
     * buffers are destroyed too.
     *
     * buffer is where to copy the buffer pointer to.
     * timeoutMs is the amount of time to wait if there is no buffer available.
     *
     * Returns:
     *  0:              on success.
     *  -EINVAL:        if buffer is null.
     *  -ETIMEDOUT:     if no buffer becomes available within the specified amount of time.
     */
    status_t getBuffer(PipelineBuffer **buffer, uint32_t timeoutMs);

    /**
     * Return a buffer to the stream that was obtained by getBuffer().
     *
     * buffer is a pointer to the returning buffer.
     *
     * Returns:
     *  0:              on success.
     *  -EINVAL:        if buffer is null.
     */
    status_t returnBuffer(PipelineBuffer *buffer);

private:
    // Private constructor. Use newPipelineStream to create a new instance of PipelineStream.
    PipelineStream();

    /**
     * Create a stream based stream configuration.
     *
     * config is the configuration to create the stream of.
     * numBuffers is the number of buffers to create for the stream.
     *
     * Returns:
     *  0:              on success.
     *  -EINVAL:        if config is not supported or invalid.
     *  -EEXIST:        if stream is already created.
     */
    status_t create(const StreamConfiguration &config, int numBuffers);

    // Destroy the stream and free all buffers with mApiLock held.
    void destroyLocked();

    // Configuration of the stream.
    StreamConfiguration mConfig;

    // Protect public methods.
    mutable std::mutex mApiLock;

    // Condition signalled when a buffer become available.
    std::condition_variable mAvailableBufferCond;

    // Buffers that are currently available for getBuffer().
    std::deque<PipelineBuffer*> mAvailableBuffers;

    // All allocated buffers.
    std::deque<std::unique_ptr<PipelineBuffer>> mAllBuffers;
};

} // namespace pbcamera

#endif // PAINTBOX_HDR_PLUS_PIPELINE_STREAM_H
