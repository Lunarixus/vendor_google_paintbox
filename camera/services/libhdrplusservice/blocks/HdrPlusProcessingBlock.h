#ifndef PAINTBOX_HDR_PLUS_PIPELINE_HDR_PLUS_PROCESSING_BLOCK_H
#define PAINTBOX_HDR_PLUS_PIPELINE_HDR_PLUS_PROCESSING_BLOCK_H

#include "PipelineBuffer.h"
#include "PipelineBlock.h"
#include "SourceCaptureBlock.h"

#include "HdrPlusProfiler.h"

#include <stdlib.h>
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
                std::weak_ptr<SourceCaptureBlock> sourceCaptureBlock);
    bool doWorkLocked() override;
    status_t flushLocked() override;

protected:
    // Set static metadata.
    status_t setStaticMetadata(std::shared_ptr<StaticMetadata> metadata);

private:
    // Use newHdrPlusProcessingBlock to create a HdrPlusProcessingBlock.
    HdrPlusProcessingBlock(std::weak_ptr<SourceCaptureBlock> sourceCaptureBlock);

    // Gcam related constants.
    static const gcam::GcamPixelFormat kGcamFinalImageFormat = gcam::GcamPixelFormat::kNv21;
    static const int32_t kGcamThreadCounts = 1;
    static const bool kGcamTuningLocked = true;
    static const int32_t kGcamFullMeteringSweepFrames = 7;
    static const int32_t kGcamMinPayloadFrames = 5;
    // TODO(b/35848231): Increase max payload frames to 7 after Easel memory leak fixed.
    static const int32_t kGcamMaxPayloadFrames = 5;
    static const gcam::PayloadFrameCopyMode kGcamPayloadFrameCopyMode =
            gcam::PayloadFrameCopyMode::kNeverCopy;
    static const int32_t kGcamRawBitsPerPixel = 10;
    static const uint32_t kGcamDebugSaveBitmask = 0;
    static constexpr float kPostRawSensitivityBoostUnity = 100.0f;

    // Camera metadata related constants.
    static constexpr float kMaxFaceScore = 100.f;
    static constexpr float kMinFaceScore = 1.f;

    // Callback invoked when Gcam releases an input image.
    class GcamInputImageReleaseCallback : public gcam::ImageReleaseCallback {
    public:
        GcamInputImageReleaseCallback(std::weak_ptr<PipelineBlock> block);
        virtual ~GcamInputImageReleaseCallback() = default;
    private:
        void Run(const int64_t image_id) const override;
        std::weak_ptr<PipelineBlock> mBlock;
    };

    // Callback invoked when Gcam finishes a final processed image.
    class GcamFinalImageCallback : public gcam::ImageCallback {
    public:
        GcamFinalImageCallback(std::weak_ptr<PipelineBlock> block);
        virtual ~GcamFinalImageCallback() = default;
    private:
        void Run(int burst_id, gcam::YuvImage* yuv_result, gcam::InterleavedImageU8* rgb_result,
                gcam::GcamPixelFormat pixel_format) const override;
        std::weak_ptr<PipelineBlock> mBlock;
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
        // Burst id.
        int32_t burstId;
        // Block output request.
        OutputRequest outputRequest;
        // A list of payload frame data.
        std::deque<std::shared_ptr<PayloadFrame>> frames;

        DECLARE_PROFILER_TIMER(timer, "HDR+ Processing");
    };

    // Callback invoked when Gcam releases an input image.
    void onGcamInputImageReleased(const int64_t imageId);

    // Callback invoked when Gcam finishes a final processed image.
    void onGcamFinalImage(int burst_id, gcam::YuvImage* yuvResult,
            gcam::InterleavedImageU8* rgbResult, gcam::GcamPixelFormat pixelFormat);

    // Initialize a Gcam instance.
    status_t initGcam();

    // Convert static metadata to Gcam static metadata.
    status_t convertToGcamStaticMetadata(std::unique_ptr<gcam::StaticMetadata> *gcamStaticMetadata,
            std::shared_ptr<StaticMetadata> metadata);

    // Issue a shot capture.
    status_t IssueShotCapture(std::shared_ptr<ShotCapture> shotCapture,
            const std::vector<Input> &inputs);

    // Add a payload frame to the shot.
    status_t addPayloadFrame(std::shared_ptr<PayloadFrame> frame, gcam::IShot *shot,
            const Input &input);

    // Get the camera channel index ({R, G_red, G_blue, B}) given a gcam channel
    // index ({R, G_even, G_odd, B})
    uint32_t getCameraChannelIndex(uint32_t gcamChannelIndex, uint8_t cfa);

    // Fill gcam frame metadata in a payload frame.
    status_t fillGcamFrameMetadata(std::shared_ptr<PayloadFrame> frame,
            const std::shared_ptr<FrameMetadata>& metadata);

    std::mutex mHdrPlusProcessingLock;

    // Static metadata of current device.
    std::shared_ptr<StaticMetadata> mStaticMetadata;

    // Gcam static metadata of current device.
    std::unique_ptr<gcam::StaticMetadata> mGcamStaticMetadata;

    // Gcam callback for releasing an input image.
    std::unique_ptr<GcamInputImageReleaseCallback> mGcamInputImageReleaseCallback;

    // Gcam callback for finishing a final image.
    std::unique_ptr<GcamFinalImageCallback> mGcamFinalImageCallback;

    // Gcam instance.
    std::unique_ptr<gcam::Gcam> mGcam;

    // Pending shot capture that is being processed in gcam.
    std::shared_ptr<ShotCapture> mPendingShotCapture;

    // Condition for shot complete.
    std::condition_variable mShotCompletedCondition;

    // TODO: Remove reference to source capture block. b/34854987
    std::weak_ptr<SourceCaptureBlock> mSourceCaptureBlock;
};

} // namespace pbcamera

#endif // PAINTBOX_HDR_PLUS_PIPELINE_HDR_PLUS_PROCESSING_BLOCK_H
