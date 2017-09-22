#ifndef PAINTBOX_HDR_PLUS_PIPELINE_HDR_PLUS_PROCESSING_BLOCK_H
#define PAINTBOX_HDR_PLUS_PIPELINE_HDR_PLUS_PROCESSING_BLOCK_H

#include "PipelineBuffer.h"
#include "PipelineBlock.h"
#include "SourceCaptureBlock.h"

#include "HdrPlusProfiler.h"

#include <stdlib.h>
#include <unordered_map>
#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/gcam.h"
#include "system/camera_metadata_tags.h"

namespace pbcamera {

/*
 * HdrPlusProcessingBlock is a pipeline block that processes a few buffers in PipelineBlock::Input
 * and writes the result image to buffers in PipelineBlock::OutputRequest. HdrPlusProcessingBlock's
 * doWorkLocked() starts its work when enough PipelineBlock::Inputs and a
 * PipelineBlock::OutputRequest is available.
 */
class HdrPlusProcessingBlock : public PipelineBlock {
public:
    virtual ~HdrPlusProcessingBlock();

    /*
     * Create a HdrPlusProcessingBlock.
     *
     * pipeline is the pipline this block belongs to.
     *
     * Returns a std::shared_ptr<HdrPlusProcessingBlock> pointing to a HdrPlusProcessingBlock on
     *         success.
     * Returns a std::shared_ptr<HdrPlusProcessingBlock> pointing to nullptr if it failed.
     */
    static std::shared_ptr<HdrPlusProcessingBlock> newHdrPlusProcessingBlock(
                std::weak_ptr<HdrPlusPipeline> pipeline, std::shared_ptr<StaticMetadata> metadata,
                std::weak_ptr<SourceCaptureBlock> sourceCaptureBlock, bool skipTimestampCheck,
                int32_t cameraId, ImxMemoryAllocatorHandle imxMemoryAllocatorHandle,
                std::shared_ptr<MessengerToHdrPlusClient> messenger);
    bool doWorkLocked() override;
    status_t flushLocked() override;

    // Return if HDR+ processing block is ready for requests.
    bool isReady();

protected:
    // Set static metadata.
    status_t setStaticMetadata(std::shared_ptr<StaticMetadata> metadata);

private:
    // Use newHdrPlusProcessingBlock to create a HdrPlusProcessingBlock.
    HdrPlusProcessingBlock(std::weak_ptr<SourceCaptureBlock> sourceCaptureBlock,
            bool skipTimestampCheck, int32_t cameraId,
            ImxMemoryAllocatorHandle imxMemoryAllocatorHandle,
            std::shared_ptr<MessengerToHdrPlusClient> messenger);

    // Gcam related constants.
    static const int32_t kGcamThreadCounts = 1;
    static const bool kGcamTuningLocked = true;
    static const int32_t kGcamFullMeteringSweepFrames = 7;
    static const int32_t kGcamMinPayloadFrames = 3;
    // Max number of frames for align/merge.
    // TODO(b/35848231): Increase max payload frames to 7 after Easel memory leak fixed.
    static const int32_t kGcamMaxPayloadFrames = 5;
    // Max number of frames as input to gcam.
    static const int32_t kGcamMaxZslFrames = 6;
    static const gcam::PayloadFrameCopyMode kGcamPayloadFrameCopyMode =
            gcam::PayloadFrameCopyMode::kNeverCopy;
    static const int32_t kGcamRawBitsPerPixel = 10;
    static constexpr float kPostRawSensitivityBoostUnity = 100.0f;
    static const bool kGcamCorrectBlackLevel = false;
    static const bool kGcamDetectFlare = false;
    static const int32_t kInvalidBaseFrameIndex = -1;
    static constexpr float kCropRatioThreshold = 0.005;
    static const gcam::GcamPixelFormat kGcamPostviewFormat = gcam::GcamPixelFormat::kRgb;
    static const uint32_t kGcamPostviewWidthBack = 168;
    static const uint32_t kGcamPostviewWidthFront = 162;
    // kGcamMaxFilenameLength must match the size of gcam::ImageSaverParams.dest_folder.
    static const int32_t kGcamMaxFilenameLength = 512;

    // Camera metadata related constants.
    static constexpr float kMaxFaceScore = 100.f;
    static constexpr float kMinFaceScore = 1.f;

    // The threshold to decide if an input is too old to be used for HDR+.
    static const int64_t kOldInputTimeThresholdNs = 1000000000; // 1 seconds.

    // Callback invoked when Gcam releases an input image.
    class GcamInputImageReleaseCallback : public gcam::ImageReleaseCallback {
    public:
        GcamInputImageReleaseCallback(std::weak_ptr<PipelineBlock> block);
        virtual ~GcamInputImageReleaseCallback() = default;
    private:
        void Run(const int64_t image_id) override;
        std::weak_ptr<PipelineBlock> mBlock;
    };

    // Callback invoked when Gcam finishes a final processed image.
    class GcamFinalImageCallback : public gcam::FinalImageCallback {
    public:
        GcamFinalImageCallback(std::weak_ptr<PipelineBlock> block);
        virtual ~GcamFinalImageCallback() = default;

        virtual void YuvReady(const gcam::IShot* shot,
                gcam::YuvImage* yuv_result,
                const gcam::ExifMetadata& metadata,
                gcam::GcamPixelFormat pixel_format) override;

        void RgbReady(const gcam::IShot* shot,
            gcam::InterleavedImageU8* image,
            const gcam::ExifMetadata& metadata,
            gcam::GcamPixelFormat pixel_format) override {};

        void PreallocatedRgbReady(const gcam::IShot* shot,
                const gcam::InterleavedReadViewU8& image_view,
                const gcam::ExifMetadata& metadata,
                gcam::GcamPixelFormat pixel_format) override {};

        void PreallocatedYuvReady(const gcam::IShot* shot,
                const gcam::YuvReadView& image_view,
                const gcam::ExifMetadata& metadata,
                gcam::GcamPixelFormat pixel_format) override {};

        std::weak_ptr<PipelineBlock> mBlock;
    };

    // Callback invoked when Gcam selects a base frame.
    class GcamBaseFrameCallback : public gcam::BaseFrameCallback {
    public:
        GcamBaseFrameCallback(std::weak_ptr<PipelineBlock> block);
        virtual ~GcamBaseFrameCallback() = default;
    private:
        virtual void Run(const gcam::IShot* shot, int base_frame_index,
                         int64_t base_frame_timestamp_ns);
        std::weak_ptr<PipelineBlock> mBlock;
    };

    // Callback invoked when Gcam generates a postview.
    class GcamPostviewCallback : public gcam::PostviewCallback {
    public:
        GcamPostviewCallback(std::weak_ptr<PipelineBlock> block);
        virtual ~GcamPostviewCallback() = default;
    private:
        virtual void Run(const gcam::IShot* shot,
                         gcam::YuvImage* yuv_result,
                         gcam::InterleavedImageU8* rgb_result,
                         gcam::GcamPixelFormat pixel_format);
        std::weak_ptr<PipelineBlock> mBlock;
    };

    // Callback invoked when Gcam requests to dump data to a file.
    class GcamFileSaver : public gcam::FileSaver {
    public:
        GcamFileSaver(std::weak_ptr<PipelineBlock> block);
        virtual ~GcamFileSaver() = default;
    private:
        std::weak_ptr<PipelineBlock> mBlock;
        virtual bool operator()(const void* data, size_t byte_count,
                              const std::string& filename) override;
    };

    // An IMX buffer wrapper for easier buffer life cycle management.
    class ImxBuffer {
    public:
        ImxBuffer();
        virtual ~ImxBuffer();
        status_t allocate(ImxMemoryAllocatorHandle imxMemoryAllocatorHandle,
                uint32_t width, uint32_t height, int32_t format);
        uint8_t *getData();
        uint32_t getWidth() const;
        uint32_t getHeight() const;
        uint32_t getStride() const;
        int32_t getFormat() const;
    private:
        ImxDeviceBufferHandle mBuffer;
        uint8_t *mData;
        uint32_t mWidth;
        uint32_t mHeight;
        uint32_t mStride;
        int32_t mFormat;
    };

    // Contains information about a payload frame for a GCam shot capture.
    struct PayloadFrame {
        // Block input.
        Input input;
        // Frame metadata.
        gcam::FrameMetadata gcamFrameMetadata;
        // Lens shading map.
        std::shared_ptr<gcam::SpatialGainMap> gcamSpatialGainMap;
    };

    // Contains information about a Gcam shot capture.
    struct ShotCapture {
        // Shot id given by gcam.
        int32_t shotId;
        // Block output request.
        OutputRequest outputRequest;
        // A list of payload frame data.
        std::deque<std::shared_ptr<PayloadFrame>> frames;
        // Base frame index;
        int32_t baseFrameIndex;

        DECLARE_PROFILER_TIMER(timer, "HDR+ Processing");
    };

    // Contains information used to send out a shutter event.
    struct Shutter {
        int32_t shotId; // Shot id for this shutter.
        int32_t baseFrameIndex; // Base frame index for this shot.
        int64_t baseFrameTimestampNs;  // Base frame timestamp for this shot.
    };

    // Contains information used to send out a postview.
    struct Postview {
        int32_t shotId; // Shot id for this postview.
        std::unique_ptr<gcam::InterleavedImageU8> rgbImage;
    };

    // Callback invoked when Gcam releases an input image.
    void onGcamInputImageReleased(const int64_t imageId);

    // Callback invoked when Gcam finishes a final processed image.
    void onGcamFinalImage(int shotId, std::unique_ptr<gcam::YuvImage> yuvResult,
            gcam::GcamPixelFormat pixelFormat, const gcam::ExifMetadata& exifMetadata);

    // Callback invoked when Gcam selects a base frame.
    void onGcamBaseFrameCallback(int shotId, int index, int64_t timestamp);

    // Callback invoked when Gcam generates a postview.
    void onGcamPostview(int32_t shotId, std::unique_ptr<gcam::YuvImage> yuvResult,
        std::unique_ptr<gcam::InterleavedImageU8> rgbResult, gcam::GcamPixelFormat pixelFormat);

    // Callback invoked when Gcam requests to dump data to a file.
    bool onGcamFileSaver(const void* data, size_t bytes, const std::string& filename);

    // Initialize a Gcam instance.
    status_t initGcam();

    // Convert static metadata to Gcam static metadata.
    status_t convertToGcamStaticMetadata(std::unique_ptr<gcam::StaticMetadata> *gcamStaticMetadata,
            std::shared_ptr<StaticMetadata> metadata);

    // Handle capture request. Must called with mHdrPlusProcessingLock locked.
    status_t handleCaptureRequestLocked(const std::vector<Input> &inputs,
            const OutputRequest &outputRequest);

    // Issue a shot capture.
    status_t IssueShotCapture(std::shared_ptr<ShotCapture> shotCapture,
            const std::vector<Input> &inputs, const OutputRequest &outputRequest);

    // Add a payload frame to the shot.
    status_t addPayloadFrame(std::shared_ptr<PayloadFrame> frame, gcam::IShot *shot,
            const Input &input);

    // Get the camera channel index ({R, G_red, G_blue, B}) given a gcam channel
    // index ({R, G_even, G_odd, B})
    uint32_t getCameraChannelIndex(uint32_t gcamChannelIndex, uint8_t cfa);

    // Fill gcam frame metadata in a payload frame.
    status_t fillGcamFrameMetadata(std::shared_ptr<PayloadFrame> frame,
            const std::shared_ptr<FrameMetadata>& metadata);

    // Fill gcam image saver parameters.
    void fillGcamImageSaverParams(gcam::ImageSaverParams *param);

    // Return an input. Must be called with mQueueLock held.
    void returnInputLocked(const std::shared_ptr<HdrPlusPipeline> &pipeline, Input *input);

    // Given an input crop resolution and output resolution, calculate the overall crop region that
    // has the same aspect ratio as the output resolution.
    status_t calculateCropRect(int32_t inputCropW, int32_t inputCropH,
            int32_t outputW, int32_t outputH, float *outputCropX0, float *outputCropY0,
            float *outputCropX1, float *outputCropY1);

    // Given input and output buffers, fill gcam shot parameters.
    status_t fillGcamShotParams(gcam::ShotParams *shotParams, gcam::GcamPixelFormat *outputFormat,
            const std::vector<Input> &inputs, const OutputRequest &outputRequest);

    // Return gcam shot callbacks.
    gcam::ShotCallbacks getShotCallbacks(bool isPostviewEnabled);

    // Produce output buffers with an HDR+ processed image.
    status_t produceRequestOutputBuffers(std::unique_ptr<gcam::YuvImage> srcYuvImage,
            PipelineBufferSet *outputBuffers);

    // Copy a YUV image to a dst YUV buffer.
    status_t copyBuffer(const std::unique_ptr<gcam::YuvImage> &srcYuvImage,
            PipelineBuffer *dstBuffer);

    // Resample a source YUV image to a destination YUV buffer.
    status_t resampleBuffer(const std::unique_ptr<gcam::YuvImage> &srcYuvImage,
            PipelineBuffer *dstBuffer);

    // Return if gcam YUV format is the same as HAL format.
    bool isTheSameYuvFormat(gcam::YuvFormat gcamFormat, int halFormat);

    // Notify AP about a shutter. Must be called when mShuttersLock is locked.
    void notifyShutterLocked(const Shutter &shutter);

    // Notify AP about a postview. Must be called when mPostviewsLock is locked.
    void notifyPostviewLocked(const Postview &postview);

    // Helper functions for managing buffer references.
    void addInputReference(int64_t bufferId, Input input);
    void removeInputReference(int64_t bufferId);

    // Assumes mInputQueue is sorted and performs insertion.
    void insertIntoInputQueue(Input input);

    std::mutex mHdrPlusProcessingLock;

    // Static metadata of current device.
    std::shared_ptr<StaticMetadata> mStaticMetadata;

    // Gcam static metadata of current device.
    std::unique_ptr<gcam::StaticMetadata> mGcamStaticMetadata;

    // Gcam callback for releasing an input image.
    std::unique_ptr<GcamInputImageReleaseCallback> mGcamInputImageReleaseCallback;

    // Gcam callback for finishing a final image.
    std::unique_ptr<GcamFinalImageCallback> mGcamFinalImageCallback;

    // Gcam callback for selecting a base frame.
    std::unique_ptr<GcamBaseFrameCallback> mGcamBaseFrameCallback;

    // Gcam callback for postview.
    std::unique_ptr<GcamPostviewCallback> mGcamPostviewCallback;

    // Gcam callback for saving a file.
    std::unique_ptr<GcamFileSaver> mGcamFileSaver;

    // Gcam instance.
    std::unique_ptr<gcam::Gcam> mGcam;

    // Pending shot capture that is being processed in gcam.
    std::shared_ptr<ShotCapture> mPendingShotCapture;

    // Condition for shot complete.
    std::condition_variable mShotCompletedCondition;

    // Messenger for shutter callback.
    std::shared_ptr<MessengerToHdrPlusClient> mMessengerToClient;

    // TODO: Remove reference to source capture block. b/34854987
    std::weak_ptr<SourceCaptureBlock> mSourceCaptureBlock;

    struct InputAndRefCount {
        InputAndRefCount(Input refInput) : input(refInput), refCount(1) {}
        Input input;
        int refCount;
    };
    std::unordered_map<int64_t, InputAndRefCount> mInputIdMap;

    std::mutex mInputIdMapLock;

    // Whether to skip timestamp check to return old input buffers.
    bool mSkipTimestampCheck;

    // 0 for back camera and 1 for front camera.
    int32_t mCameraId;

    std::mutex mShuttersLock;
    std::deque<Shutter> mShutters; // Shutters ready to send to AP.

    // IMX memory allocate handle to allocate IMX buffers.
    ImxMemoryAllocatorHandle mImxMemoryAllocatorHandle;

    std::mutex mPostviewsLock;
    std::deque<Postview> mPostviews;
};

} // namespace pbcamera

#endif // PAINTBOX_HDR_PLUS_PIPELINE_HDR_PLUS_PROCESSING_BLOCK_H
