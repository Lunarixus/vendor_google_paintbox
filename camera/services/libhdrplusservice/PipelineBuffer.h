#ifndef PAINTBOX_HDR_PLUS_PIPELINE_BUFFER_H
#define PAINTBOX_HDR_PLUS_PIPELINE_BUFFER_H

#include <memory>
#include <stdint.h>
#include <vector>

#include "hardware/gchips/paintbox/system/include/capture.h"

#include "HdrPlusTypes.h"

namespace pbcamera {

class PipelineStream;
class PipelineBlock;

/**
 * PipelineBuffer
 *
 * PipelineBuffer defines image buffers that are used in HDR+ service pipeline. Each PipelineBuffer
 * belongs to a PipelineStream.
 */
class PipelineBuffer {
public:
    PipelineBuffer(const std::weak_ptr<PipelineStream> &stream,
            const StreamConfiguration &config);
    virtual ~PipelineBuffer();

    /*
     * Allocate the image data.
     *
     * Returns:
     *  0:          on success.
     *  -EINVAL:    if the stream configuration is invalid or not supported.
     *  -EEXIST:    if the image data is already allocated.
     */
    virtual status_t allocate() = 0;

    // Return width of the image.
    int32_t getWidth() const;

    // Return height of the image.
    int32_t getHeight() const;

    // Return format of the image.
    int32_t getFormat() const;

    // Return stride in bytes of an image plane.
    int32_t getStride(uint32_t planeNum) const;

    // Return the pointer to the raw data of an image plane.
    virtual uint8_t* getPlaneData(uint32_t planeNum) = 0;

    // Return the size of the allocated data including padding.
    virtual uint32_t getDataSize() const = 0;

    // Lock data of the frame buffer. It must be called before calling getPlaneData() to access
    // the plane data. After accessc finishes, call unlockData to unlock data.
    virtual status_t lockData() = 0;

    // Unlock the data of the frame buffer.
    virtual void unlockData() = 0;

    // Get the file descriptor for this buffer.
    virtual int getFd() = 0;

    // Set each pixel to black.
    status_t clear();

    // Return the stream that this image belongs to.
    std::weak_ptr<PipelineStream> getStream() const;

    // Set the block where the image is currently in.
    void setPipelineBlock(std::weak_ptr<PipelineBlock> block);

    // Set the block where the image is currently in to nullptr to indicate it's not in any block.
    void resetPipelineBlock();

    // Return the block where the image is currently in.
    std::weak_ptr<PipelineBlock> getPipelineBlock() const;

protected:
    // Sanity check the plane configuration.
    status_t validatePlaneConfig(const ImageConfiguration &image, uint32_t planeNum);

    // Sanity check the stream configuration.
    status_t validateConfig(const StreamConfiguration &config);

    // Allocated stream configuration for this buffer.
    StreamConfiguration mAllocatedConfig;

    // Requested stream configuration to allocate the buffer.
    StreamConfiguration mRequestedConfig;
    // The stream that this buffer belongs to.
    const std::weak_ptr<PipelineStream> mStream;
    // The block where is buffer is currently in. Null if it's in the stream.
    std::weak_ptr<PipelineBlock> mBlock;

private:
    static const uint8_t kClearRawValue = 0x0;
    static const uint8_t kClearLumaValue = 0x0;
    static const uint8_t kClearChromaValue = 0x80;
    int calculateBufferSize(int32_t width, int32_t height, int32_t format);
};

/**
 * PipelineImxBuffer
 *
 * PipelineImxBuffer inherited from PipelineBuffer defines HDR+ buffers allocated using IMX API.
 */
class PipelineImxBuffer : public PipelineBuffer {
public:
    PipelineImxBuffer(const std::weak_ptr<PipelineStream> &stream,
            const StreamConfiguration &config);
    virtual ~PipelineImxBuffer();

    // Use ImxMemoryAllocatorHandle to allocate a buffer instead.
    virtual status_t allocate() override;

     // Allocate a IMX buffer.
    status_t allocate(ImxMemoryAllocatorHandle imxMemoryAllocatorHandle);

    // Return the pointer to the raw data of an image plane.
    virtual uint8_t* getPlaneData(uint32_t planeNum) override;

    // Return the size of the data.
    virtual uint32_t getDataSize() const override;

    // Get the file descriptor for this buffer.
    virtual int getFd() override;

    // Lock data of the frame buffer. It must be called before calling getPlaneData() to access
    // the plane data. After accessc finishes, call unlockData to unlock data.
    virtual status_t lockData() override;

    // Unlock the data of the frame buffer.
    virtual void unlockData() override;

private:
    // Raw data of the image.
    ImxDeviceBufferHandle mImxDeviceBufferHandle;
    void* mLockedData; // pointer to access data when it's locked.
    uint32_t mDataSize;
};

/**
 * PipelineCaptureFrameBuffer
 *
 * PipelineCaptureFrameBuffer inherited from PipelineBuffer defines HDR+ buffers allocated using
 * capture Easel capture API in order to capture frames from MIPI.
 */
class PipelineCaptureFrameBuffer : public PipelineBuffer {
public:
    PipelineCaptureFrameBuffer(const std::weak_ptr<PipelineStream> &stream,
            const StreamConfiguration &config);
    virtual ~PipelineCaptureFrameBuffer() = default;

    // Use allocate(std::unique_ptr<paintbox::CaptureFrameBufferFactory> &bufferFactory) to allocate buffers.
    virtual status_t allocate() override;

    // Allocate the image data using paintbox::CaptureFrameBufferFactory.
    status_t allocate(std::unique_ptr<paintbox::CaptureFrameBufferFactory> &bufferFactory);

    // Return the pointer to the raw data of an image plane.
    virtual uint8_t* getPlaneData(uint32_t planeNum) override;

    // Return the size of the data.
    virtual uint32_t getDataSize() const override;

    // Get the file descriptor for this buffer.
    virtual int getFd() override;

    // Lock data of the frame buffer. It must be called before calling getPlaneData() to access
    // the plane data. After accessc finishes, call unlockData to unlock data.
    virtual status_t lockData() override;

    // Unlock the data of the frame buffer.
    virtual void unlockData() override;

    // Return the pointer to capture frame buffer associated with this buffer. This does not
    // transfer buffer ownership.
    paintbox::CaptureFrameBuffer *getCaptureFrameBuffer();

private:
    std::unique_ptr<paintbox::CaptureFrameBuffer> mCaptureFrameBuffer;
    void* mLockedData;
};

} // namespace pbcamera

#endif // PAINTBOX_HDR_PLUS_PIPELINE_BUFFER_H
