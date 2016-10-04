//#define LOG_NDEBUG 0
#define LOG_TAG "HdrPlusProcessingBlock"
#include <utils/Log.h>

#include <inttypes.h>
#include <system/graphics.h>

#include "googlex/gcam/gcam/src/lib_gcam/shot_interface.h"

#include "HdrPlusProcessingBlock.h"
#include "HdrPlusPipeline.h"

namespace pbcamera {

HdrPlusProcessingBlock::HdrPlusProcessingBlock() :
        PipelineBlock("HdrPlusProcessingBlock") {
}

HdrPlusProcessingBlock::~HdrPlusProcessingBlock() {
}

std::shared_ptr<HdrPlusProcessingBlock> HdrPlusProcessingBlock::newHdrPlusProcessingBlock(
        std::weak_ptr<HdrPlusPipeline> pipeline, std::shared_ptr<StaticMetadata> metadata) {
    ALOGV("%s", __FUNCTION__);

    auto block = std::shared_ptr<HdrPlusProcessingBlock>(new HdrPlusProcessingBlock());
    if (block == nullptr) {
        ALOGE("%s: Failed to create a block instance.", __FUNCTION__);
        return nullptr;
    }

    status_t res = block->create(pipeline);
    if (res != 0) {
        ALOGE("%s: Failed to create block %s", __FUNCTION__, block->getName());
        return nullptr;
    }

    res = block->setStaticMetadata(metadata);
    if (res != 0) {
        ALOGE("%s: Failed to set static metadata %s", __FUNCTION__, block->getName());
        return nullptr;
    }

    return block;
}

bool HdrPlusProcessingBlock::doWorkLocked() {
    ALOGV("%s", __FUNCTION__);

    std::vector<Input> inputs;
    OutputRequest outputRequest = {};

    // Check if there is a pending Gcam shot capture.
    std::unique_lock<std::mutex> lock(mHdrPlusProcessingLock);
    if (mPendingShotCapture != nullptr) {
        // Only support 1 active processing.
        return false;
    }

    // Check if we have enough input and output request.
    {
        std::unique_lock<std::mutex> lock(mQueueLock);

        auto pipeline = mPipeline.lock();
        if (pipeline == nullptr) {
            ALOGE("%s: Pipeline is destroyed.", __FUNCTION__);
            return false;
        }

        // If we have more inputs than we need, remove the oldest ones.
        while (mInputQueue.size() > kGcamMaxPayloadFrames) {
            pipeline->inputDone(mInputQueue[0]);
            mInputQueue.pop_front();
        }

        if (mInputQueue.size() < kGcamMinPayloadFrames) {
            // Nothing to do this time.
            ALOGV("%s: Not enough inputs (%zu but need %d).", __FUNCTION__, mInputQueue.size(),
                    kGcamMinPayloadFrames);
            return false;
        } else if (mOutputRequestQueue.size() == 0) {
            // Nothing to do this time.
            ALOGV("%s: No output request", __FUNCTION__);
            return false;
        }

        // Get all inputs.
        while (mInputQueue.size() > 0) {
            inputs.push_back(mInputQueue[0]);
            mInputQueue.pop_front();
        }

        outputRequest = mOutputRequestQueue[0];
        mOutputRequestQueue.pop_front();
    }

    auto shotCapture = std::make_shared<ShotCapture>();
    shotCapture->burstId = outputRequest.metadata.requestId;

    // Start a HDR+ shot.
    status_t res = IssueShotCapture(shotCapture, inputs);
    if (res != 0) {
        ALOGE("%s: Issuing a HDR+ capture failed: %s (%d).", __FUNCTION__, strerror(-res), res);

        // Push inputs and output request back to the front of the queue.
        std::unique_lock<std::mutex> lock(mQueueLock);
        mInputQueue.insert(mInputQueue.begin(), inputs.begin(), inputs.end());
        mOutputRequestQueue.push_front(outputRequest);
        return false;
    }

    shotCapture->outputRequest = outputRequest;
    mPendingShotCapture = shotCapture;

    return true;
}

status_t HdrPlusProcessingBlock::IssueShotCapture(std::shared_ptr<ShotCapture> shotCapture,
        const std::vector<Input> &inputs) {
    if (mGcam == nullptr) {
        ALOGE("%s: GCAM is not initialized.", __FUNCTION__);
        return -ENODEV;
    }

    if (mStaticMetadata == nullptr) {
        ALOGE("%s: static metadata is not initialized.", __FUNCTION__);
        return -ENODEV;
    }

    if (shotCapture == nullptr) {
        ALOGE("%s: shotCapture is nullptr.", __FUNCTION__);
        return -EINVAL;
    }

    gcam::ShotParams shotParams;
    shotParams.ae.payload_frame_orig_width = mStaticMetadata->pixelArraySize[0];
    shotParams.ae.payload_frame_orig_height = mStaticMetadata->pixelArraySize[1];
    shotParams.ae.process_bayer_for_payload = true;
    // TODO: When we support capture requests with timestamps, we may want to change this to
    // the index of the frame that has the closest timestamp.
    shotParams.zsl_base_frame_index_hint = 0;
    // TODO: support non-ZSL.
    shotParams.zsl = true;

    // camera_id is always 0 because we only set 1 static metadata in GCAM for current camera
    // which could be rear or front camera.
    gcam::IShot* shot = mGcam->StartShotCapture(/*camera_id*/0, shotCapture->burstId,
            shotParams, /*postview_params*/nullptr, /*debug_save_params*/nullptr);
    if (shot == nullptr) {
        ALOGE("%s: Failed to start a shot capture.", __FUNCTION__);
        return -ENODEV;
    }

    // Begin payload frame with an empty burst spec for ZSL.
    gcam::BurstSpec burstSpec;
    shot->BeginPayloadFrames(burstSpec);

    // Add all payload frames to the shot.
    for (auto input : inputs) {
        auto frame = std::make_shared<PayloadFrame>();
        status_t res = addPayloadFrame(frame, shot, input);
        if (res != 0) {
            ALOGE("%s: Failed to add a payload frame: %s (%d).", __FUNCTION__, strerror(-res), res);
            mGcam->AbortShotCapture(shot);
            return res;
        }
        shotCapture->frames.push_back(frame);
    }

    // End payload frames.
    if (!shot->EndPayloadFrames(/*client_exif_metadata*/nullptr,
            /*general_warnings*/nullptr, /*general_errors*/nullptr)) {
        ALOGE("%s: Failed to end payload frames.", __FUNCTION__);
        mGcam->AbortShotCapture(shot);
        return -ENODEV;
    }

    // End shot capture.
    if (!mGcam->EndShotCapture(shot)) {
        ALOGE("%s: Failed to end a shot capture.", __FUNCTION__);
        return -ENODEV;
    }

    return 0;
}

status_t HdrPlusProcessingBlock::addPayloadFrame(std::shared_ptr<PayloadFrame> frame,
        gcam::IShot *shot, const Input &input) {
    if (frame == nullptr) {
        ALOGE("%s: gcamFrameData is nullptr.", __FUNCTION__);
        return -EINVAL;
    }
    if (shot == nullptr) {
        ALOGE("%s: shot is nullptr.", __FUNCTION__);
        return -EINVAL;
    }
    // Make sure each input only has 1 buffer.
    if (input.buffers.size() != 1) {
        ALOGE("%s: Expecting 1 buffer in the input but there are %zu.", __FUNCTION__,
                input.buffers.size());
        return -EINVAL;
    }

    // Fill gcam metadata
    status_t res = fillGcamFrameMetadata(frame, input.metadata.frameMetadata);
    if (res != 0) {
        ALOGE("%s: Converting to GCam frame metadata failed: %s (%d).", __FUNCTION__,
                strerror(-res), res);
        return res;
    }

    gcam::RawBufferLayout layout;
    int32_t widthBytes = 0;
    switch (input.buffers[0]->getFormat()) {
        case HAL_PIXEL_FORMAT_RAW10:
            layout = gcam::RawBufferLayout::kRaw10;
            widthBytes = input.buffers[0]->getWidth() * 10 / 8;
            break;
        case HAL_PIXEL_FORMAT_RAW16:
            layout = gcam::RawBufferLayout::kRaw16;
            widthBytes = input.buffers[0]->getWidth() * 2;
            break;
        default:
            ALOGE("%s: Unknown format 0x%x.", __FUNCTION__, input.buffers[0]->getFormat());
            return -EINVAL;
    }

    // Create a gcam RAW image.
    gcam::RawWriteView raw(input.buffers[0]->getWidth(), input.buffers[0]->getHeight(),
            input.buffers[0]->getStride() - widthBytes, layout, input.buffers[0]->getData());
    if (!shot->AddPayloadFrame(frame->gcamFrameMetadata,
            /*raw_id*/(uintptr_t)input.buffers[0]->getData(), raw,
            *frame->gcamSpatialGainMap.get(), *frame->gcamSpatialGainMap.get())) {
        ALOGE("%s: Adding a payload frame failed.", __FUNCTION__);
        return -ENODEV;
    }

    frame->input = input;

    return 0;
}

void HdrPlusProcessingBlock::onGcamInputImageReleased(const int64_t imageId) {
    ALOGD("%s: Got image %" PRId64, __FUNCTION__, imageId);
    // TODO: Put buffers back to queue here instead of later in onGcamFinalImage().
}

void HdrPlusProcessingBlock::onGcamFinalImage(int burst_id, gcam::YuvImage* yuvResult,
        gcam::InterleavedImageU8* rgbResult, gcam::GcamPixelFormat pixelFormat) {
    ALOGD("%s: Got a final image (format %d) for request %d.", __FUNCTION__, pixelFormat, burst_id);

    if (rgbResult != nullptr) {
        ALOGW("%s: Not expecting an RGB final image from GCAM.", __FUNCTION__);
    }

    if (yuvResult == nullptr) {
        ALOGE("%s: Expecting a YUV final image but yuvResult is nullptr.", __FUNCTION__);
        return;
    }

    if (pixelFormat != kGcamFinalImageFormat) {
        ALOGE("%s: Expecting format %d but got format %d. Dropping this result.", __FUNCTION__,
                kGcamFinalImageFormat, pixelFormat);
        return;
    }

    OutputResult outputResult = {};
    std::shared_ptr<ShotCapture> finishingShot;
    {
        std::unique_lock<std::mutex> lock(mHdrPlusProcessingLock);

        if (mPendingShotCapture == nullptr) {
            ALOGE("%s: There is no pending shot for burst id %d. Dropping a final image.",
                    __FUNCTION__, burst_id);
            return;
        }

        if (burst_id != mPendingShotCapture->burstId) {
            ALOGE("%s: Expecting a final image for shot %d but got a final image for shot %d.",
                    __FUNCTION__, mPendingShotCapture->burstId, burst_id);
            return;
        }

        finishingShot = mPendingShotCapture;
        mPendingShotCapture = nullptr;
    }

    // Copy HDR+ processed final image to block output buffers. This won't be needed for PB
    // version.
    for (auto outputBuffer : finishingShot->outputRequest.buffers) {
        // Clear the buffer first because GCAM's final image resolution may be smaller.
        // This won't be needed for PB version.
        outputBuffer->clear();
        uint8_t *data = outputBuffer->getData();

        // The following assumes format is NV21.
        if (outputBuffer->getFormat() != HAL_PIXEL_FORMAT_YCrCb_420_SP) {
            ALOGE("%s: Only NV21 output buffer is supported.", __FUNCTION__);
            break;
        }

        // Copy luma line by line from the final image.
        const gcam::InterleavedImageU8 &lumaImageSrc = yuvResult->luma_image();
        int32_t lineBytesToCopy = std::min(outputBuffer->getWidth(), lumaImageSrc.width());
        uint32_t linesToCopy = std::min(outputBuffer->getHeight(), lumaImageSrc.height());
        for (uint32_t y = 0; y < linesToCopy; y++) {
            std::memcpy(data + y * outputBuffer->getStride(), &lumaImageSrc.at(0, y, 0),
                    lineBytesToCopy);
        }

        // Copy chroma line by line from the final image.
        const gcam::InterleavedImageU8 &chromaImageSrc = yuvResult->chroma_image();
        uint8_t* chromaDst = data + outputBuffer->getHeight() * outputBuffer->getStride();
        lineBytesToCopy = std::min(outputBuffer->getWidth() , chromaImageSrc.width() * 2);
        linesToCopy = std::min(outputBuffer->getHeight() / 2, chromaImageSrc.height());
        for (uint32_t y = 0; y < linesToCopy; y++) {
            // TODO: This assumes chroma has the same stride as luma. This is currently fine.
            // The assumption may not be true for QC HAL, Easel or PB HW, and should be fixed.
            // b/31623156
            std::memcpy(chromaDst + y * outputBuffer->getStride(), &chromaImageSrc.at(0, y, 0),
                    lineBytesToCopy);
        }

        outputResult.buffers.push_back(outputBuffer);
    }

    // Set frame metadata.
    outputResult.metadata.frameMetadata =
            finishingShot->frames[0]->input.metadata.frameMetadata;

    // Set request ID.
    outputResult.metadata.requestId = finishingShot->outputRequest.metadata.requestId;

    auto pipeline = mPipeline.lock();
    if (pipeline == nullptr) {
        ALOGE("%s: Pipeline is destroyed.", __FUNCTION__);
        return;
    }

    // Check if we got all output buffers.
    if (finishingShot->outputRequest.buffers.size() != outputResult.buffers.size()) {
        ALOGE("%s: Processed %zu output buffers but expecting %zu.", __FUNCTION__,
                outputResult.buffers.size(), finishingShot->outputRequest.buffers.size());

        // Abort output request.
        pipeline->outputRequestAbort(finishingShot->outputRequest);
        // TODO: Notify the client about the failed request.

        // Continue to return input buffers.
    } else {
        // Send out output result.
        pipeline->outputDone(outputResult);
    }

    // Return input buffers back to the input queue.
    std::vector<Input> inputs;
    for (auto &frame : finishingShot->frames) {
        inputs.push_back(frame->input); // Create a vector of inputs to keep the order right.
    }

    // Push inputs back to the front of the queue because it may be needed for next capture.
    {
        std::unique_lock<std::mutex> lock(mQueueLock);
        mInputQueue.insert(mInputQueue.begin(), inputs.begin(), inputs.end());
    }

    // Notify worker thread that it can start next processing.
    notifyWorkerThreadEvent();
}

status_t HdrPlusProcessingBlock::convertToGcamStaticMetadata(
        std::unique_ptr<gcam::StaticMetadata> *gcamStaticMetadata,
        std::shared_ptr<StaticMetadata> metadata) {
    auto gcamMetadata = std::make_unique<gcam::StaticMetadata>();
    gcamMetadata->has_flash = metadata->flashInfoAvailable;
    gcamMetadata->iso_range[0] = metadata->sensitivityRange[0];
    gcamMetadata->iso_range[1] = metadata->sensitivityRange[1];
    gcamMetadata->max_analog_iso = metadata->maxAnalogSensitivity;
    gcamMetadata->pixel_array_width = metadata->pixelArraySize[0];
    gcamMetadata->pixel_array_height = metadata->pixelArraySize[1];
    gcamMetadata->active_area.x0 = metadata->activeArraySize[0];
    gcamMetadata->active_area.y0 = metadata->activeArraySize[1];
    gcamMetadata->active_area.x1 = metadata->activeArraySize[0] + metadata->activeArraySize[2];
    gcamMetadata->active_area.y1 = metadata->activeArraySize[1] + metadata->activeArraySize[3];

    for (auto &region : metadata->opticalBlackRegions) {
        gcam::PixelRect r;
        r.x0 = region[0];
        r.y0 = region[1];
        r.x1 = region[0] + region[2];
        r.y1 = region[1] + region[3];
        gcamMetadata->optically_black_regions.push_back(r);
    }

    gcamMetadata->frame_raw_max_width = metadata->pixelArraySize[0];
    gcamMetadata->frame_raw_max_height = metadata->pixelArraySize[1];
    gcamMetadata->raw_bits_per_pixel = kGcamRawBitsPerPixel;

    gcam::ColorCalibration colorCalibration[2];
    colorCalibration[0].illuminant =
            static_cast<gcam::ColorCalibration::Illuminant>(metadata->referenceIlluminant1);
    colorCalibration[1].illuminant =
            static_cast<gcam::ColorCalibration::Illuminant>(metadata->referenceIlluminant2);
    for (uint32_t i = 0; i < 9; i++) {
        colorCalibration[0].xyz_to_model_rgb[i] = metadata->colorTransform1[i];
        colorCalibration[0].model_rgb_to_device_rgb[i] = metadata->calibrationTransform1[i];
        colorCalibration[1].xyz_to_model_rgb[i] = metadata->colorTransform2[i];
        colorCalibration[1].model_rgb_to_device_rgb[i] = metadata->calibrationTransform2[i];
    }

    gcamMetadata->color_calibration.push_back(colorCalibration[0]);
    gcamMetadata->color_calibration.push_back(colorCalibration[1]);
    gcamMetadata->white_level = metadata->whiteLevel;
    switch (metadata->colorFilterArrangement) {
        case ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGGB:
            gcamMetadata->bayer_pattern = gcam::BayerPattern::kRGGB;
            break;
        case ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GRBG:
            gcamMetadata->bayer_pattern = gcam::BayerPattern::kGRBG;
            break;
        case ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GBRG:
            gcamMetadata->bayer_pattern = gcam::BayerPattern::kGBRG;
            break;
        case ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_BGGR:
            gcamMetadata->bayer_pattern = gcam::BayerPattern::kBGGR;
            break;
        default:
            ALOGE("%s: Unknown color filter arrangement: %d.", __FUNCTION__,
                    metadata->colorFilterArrangement);
            return -EINVAL;
    }

    gcamMetadata->available_f_numbers = metadata->availableApertures;
    gcamMetadata->available_focal_lengths_mm = metadata->availableFocalLengths;

    // TODO: Get make, model, device from somewhere.
    static const char* kGcamMake = "Google";
    static const char* kGcamDevice = "Marlin";
    gcamMetadata->make = std::string(kGcamMake);
    gcamMetadata->device = std::string(kGcamDevice);

    if (gcamStaticMetadata != nullptr) {
        *gcamStaticMetadata = std::move(gcamMetadata);
    }

    return 0;
}

// Gcam channel order {R, G_red, G_blue, B} => Camera channel order {R, G_even, G_odd, B}
uint32_t HdrPlusProcessingBlock::getCameraChannelIndex(uint32_t gcamChannelIndex, uint8_t cfa) {
    switch (gcamChannelIndex) {
        // R -> R
        case 0:
        // G -> G
        case 3:
            return gcamChannelIndex;
        case 1:
        case 2:
            switch (cfa) {
                case ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGGB:
                case ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GRBG:
                    // G_red -> G_even and G_blue -> G_odd
                    return gcamChannelIndex;
                case ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GBRG:
                case ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_BGGR:
                    // G_red -> G_odd and G_blue -> G_even
                    return gcamChannelIndex == 1 ? 2 : 1;
            }
        default:
            // This should not happen.
            ALOGE("%s: Invalid GCAM channel index %u or color filter arrangement %d.",
                    __FUNCTION__, gcamChannelIndex, cfa);
            return 0;
    }
}

status_t HdrPlusProcessingBlock::fillGcamFrameMetadata(std::shared_ptr<PayloadFrame> frame,
            const std::shared_ptr<FrameMetadata>& metadata) {
    if (frame == nullptr) {
        ALOGE("%s: frame is nullptr.", __FUNCTION__);
        return -EINVAL;
    }

    gcam::FrameMetadata *gcamMetadata = &frame->gcamFrameMetadata;
    gcamMetadata->actual_exposure_time_ms = metadata->exposureTime / 1000000.0f; // ns to ms.

    // Assume analog gain is used in the total sensitivity first and then digital gain for the rest.
    if (metadata->sensitivity > mStaticMetadata->maxAnalogSensitivity) {
        gcamMetadata->actual_analog_gain =
                static_cast<float>(mStaticMetadata->maxAnalogSensitivity) /
                mStaticMetadata->sensitivityRange[0];
        gcamMetadata->applied_digital_gain =
                std::max(static_cast<float>(metadata->sensitivity) /
                mStaticMetadata->maxAnalogSensitivity, 1.0f);
    } else {
        gcamMetadata->actual_analog_gain = static_cast<float>(metadata->sensitivity) /
                mStaticMetadata->sensitivityRange[0];
        gcamMetadata->applied_digital_gain = 1.0f;
    }

    gcamMetadata->post_raw_digital_gain =
            metadata->postRawSensitivityBoost / kPostRawSensitivityBoostUnity;
    gcamMetadata->flash =
            (metadata->flashMode == ANDROID_FLASH_MODE_SINGLE ||
             metadata->flashMode == ANDROID_FLASH_MODE_TORCH) ?
            gcam::FlashMetadata::kOn : gcam::FlashMetadata::kOff;
    gcamMetadata->wb_capture.color_temp = gcam::kColorTempUnknown;

    // Remap Camera2 order {R, G_even, G_odd, B} to Gcam order {R, GR, GB, B}
    uint8_t cfa = mStaticMetadata->colorFilterArrangement;
    for (uint32_t i = 0; i < 4; i++) {
        gcamMetadata->wb_capture.gains[i] =
                metadata->colorCorrectionGains[getCameraChannelIndex(i, cfa)] *
                gcam::kWbGainUnityValue;
    }

    for (uint32_t i = 0; i < 9; i ++) {
        gcamMetadata->wb_capture.rgb2rgb[i] = metadata->colorCorrectionTransform[i];
    }
    gcamMetadata->wb_ideal = gcamMetadata->wb_capture;
    for (uint32_t i = 0; i < 3; i++) {
        gcamMetadata->neutral_point [i] = metadata->neutralColorPoint[i];
    }

    gcamMetadata->sensor_temp = gcam::kSensorTempUnknown;
    gcamMetadata->timestamp_ns = metadata->timestamp;
    gcamMetadata->was_black_level_locked = metadata->blackLevelLock;
    gcamMetadata->sensor_id = 0;
    switch (metadata->sceneFlicker) {
        case ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF:
            gcamMetadata->scene_flicker = gcam::SceneFlicker::kNone;
            break;
        case ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ:
            gcamMetadata->scene_flicker = gcam::SceneFlicker::k50Hz;
            break;
        case ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ:
            gcamMetadata->scene_flicker = gcam::SceneFlicker::k60Hz;
            break;
        default:
            gcamMetadata->scene_flicker = gcam::SceneFlicker::kUnknown;
            break;
    }
    for (uint32_t i = 0; i < 4; i++) {
        gcamMetadata->dng_noise_model_bayer[i].scale = metadata->noiseProfile[i][0];
        gcamMetadata->dng_noise_model_bayer[i].offset = metadata->noiseProfile[i][1];
    }

    for (uint32_t i = 0; i < 4; i++) {
        gcamMetadata->black_levels_bayer[i] = metadata->dynamicBlackLevel[i];
    }

    // Only use focus distance if the device's focus is reasonably calibrated.
    if (mStaticMetadata->focusDistanceCalibration ==
                ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_APPROXIMATE ||
        mStaticMetadata->focusDistanceCalibration ==
                ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_CALIBRATED) {
        gcamMetadata->focus_distance_diopters = metadata->focusDistance;
    }

    // Check numbers of face information match.
    if (metadata->faceIds.size() != metadata->faceRectangles.size() ||
        metadata->faceIds.size() != metadata->faceScores.size()) {
        ALOGE("%s: The numbers of face information don't match: face IDs: %zu, "
                "face rectangles: %zu, face scores: %zu.", __FUNCTION__,
                metadata->faceIds.size(), metadata->faceRectangles.size(),
                metadata->faceScores.size());
        return -EINVAL;
    }

    // Convert face info.
    float arrayW = static_cast<float>(mStaticMetadata->activeArraySize[2]);
    float arrayH = static_cast<float>(mStaticMetadata->activeArraySize[3]);
    for (uint32_t i = 0; i < metadata->faceIds.size(); i++) {
        const std::array<int32_t, 4> &faceRect = metadata->faceRectangles[i];
        const uint8_t &faceScore = metadata->faceScores[i];

        gcam::FaceInfo faceInfo;

        // Find the center point of the face rectangle in a [0..1],[0..1] rectangle.
        faceInfo.pos_x = (faceRect[0] + faceRect[2]) / 2.0f / arrayW;
        faceInfo.pos_y = (faceRect[1] + faceRect[3]) / 2.0f / arrayH;

        // Make the face into a square by averaging the dimensions.
        float faceRectDim =
                static_cast<float>(faceRect[2] - faceRect[0] + faceRect[3] - faceRect[1]) / 2;

        // Gcam is expecting a square whose edge length is relative to the longer axis of the image.
        if (arrayW > arrayH) {
            faceInfo.size = faceRectDim / arrayW;
        } else {
            faceInfo.size = faceRectDim / arrayH;
        }

        faceInfo.confidence = (faceScore - kMinFaceScore) / (kMaxFaceScore - kMinFaceScore);

        gcamMetadata->faces.push_back(faceInfo);
    }

    // Convert lens shading map.
    uint32_t smWidth = mStaticMetadata->shadingMapSize[0];
    uint32_t smHeight = mStaticMetadata->shadingMapSize[1];
    uint32_t numElementsPerChannel = smWidth * smHeight;

    frame->gcamSpatialGainMap = std::make_shared<gcam::SpatialGainMap>(smWidth, smHeight,
            /*is_precise*/true);

    for (uint32_t c = 0; c < 4; c++) {
        for (uint32_t y = 0; y < smHeight; y++) {
            for (uint32_t x = 0; x < smWidth; x++) {
                uint32_t index = y * smWidth + x + getCameraChannelIndex(c, cfa) *
                        numElementsPerChannel;
                frame->gcamSpatialGainMap->WriteRggb(x, y, c, metadata->lensShadingMap[index]);
            }
        }
    }

    return 0;
}

status_t HdrPlusProcessingBlock::initGcam() {
    // Create gcam callbacks.
    mGcamInputImageReleaseCallback =
            std::make_unique<GcamInputImageReleaseCallback>(shared_from_this());
    mGcamFinalImageCallback =
            std::make_unique<GcamFinalImageCallback>(shared_from_this());

    // Set up gcam init params.
    gcam::InitParams initParams;
    initParams.thread_count = kGcamThreadCounts;
    initParams.tuning_locked = kGcamTuningLocked;
    initParams.planning_to_provide_both_yuv_and_raw_for_metering = false;
    initParams.planning_to_provide_both_yuv_and_raw_for_payload = false;
    initParams.planning_to_process_bayer_for_metering = false;
    initParams.planning_to_process_bayer_for_payload = true;
    initParams.max_full_metering_sweep_frames = kGcamFullMeteringSweepFrames;
    initParams.min_payload_frames = kGcamMinPayloadFrames;
    initParams.payload_frame_copy_mode = kGcamPayloadFrameCopyMode;
    initParams.image_release_callback = mGcamInputImageReleaseCallback.get();
    initParams.final_image_callback = mGcamFinalImageCallback.get();
    initParams.final_image_pixel_format = kGcamFinalImageFormat;

    // The following callbacks are not used.
    initParams.memory_callback = nullptr;
    initParams.merge_queue_empty_callback = nullptr;
    initParams.finish_queue_empty_callback = nullptr;
    initParams.background_ae_results_callback = nullptr;
    initParams.postview_callback = nullptr;
    initParams.merged_dng_callback = nullptr;
    initParams.jpeg_callback = nullptr;
    initParams.progress_callback = nullptr;
    initParams.finished_callback = nullptr;

    // There is only 1 static metadata for current device.
    std::vector<gcam::StaticMetadata> gcamMetadataList = {*mGcamStaticMetadata};

    gcam::DebugParams debugParams;
    debugParams.save_bitmask = kGcamDebugSaveBitmask;

    // Create a gcam instance.
    mGcam = std::unique_ptr<gcam::Gcam>(gcam::Gcam::Create(initParams, gcamMetadataList,
            &debugParams));
    if (mGcam == nullptr) {
        ALOGE("%s: Failed to create a Gcam instance.", __FUNCTION__);
        mGcamInputImageReleaseCallback = nullptr;
        mGcamFinalImageCallback = nullptr;
        return -ENODEV;
    }

    return 0;
}

status_t HdrPlusProcessingBlock::setStaticMetadata(std::shared_ptr<StaticMetadata> metadata) {
    if (metadata == nullptr) {
        ALOGE("%s: metadata is nullptr.", __FUNCTION__);
        return -EINVAL;
    }

    std::unique_lock<std::mutex> lock(mHdrPlusProcessingLock);
    if (mStaticMetadata != nullptr) {
        ALOGE("%s: Static metadata already exists.", __FUNCTION__);
        return -EINVAL;
    }

    // Convert to gcam static metadata.
    status_t res = convertToGcamStaticMetadata(&mGcamStaticMetadata, metadata);
    if (res != 0) {
        ALOGE("%s: Converting to GCAM static metadata failed: %s (%d).", __FUNCTION__,
                strerror(-res), res);
        return res;
    }

    // Initialize Gcam with the static metadata.
    res = initGcam();
    if (res != 0) {
        ALOGE("%s: Initializing Gcam failed: %s (%d).", __FUNCTION__, strerror(-res), res);
        mGcamStaticMetadata = nullptr;
        return -ENODEV;
    }

    mStaticMetadata = metadata;
    return 0;
}

HdrPlusProcessingBlock::GcamInputImageReleaseCallback::GcamInputImageReleaseCallback(
        std::weak_ptr<PipelineBlock> block) : mBlock(block) {
}

void HdrPlusProcessingBlock::GcamInputImageReleaseCallback::Run(const int64_t image_id) const {
    ALOGV("%s: Gcam released an image (id %" PRId64 ").", __FUNCTION__, image_id);
    auto block = std::static_pointer_cast<HdrPlusProcessingBlock>(mBlock.lock());
    if (block != nullptr) {
        block->onGcamInputImageReleased(image_id);
    } else {
        ALOGE("%s: Gcam release an image (id %" PRId64 ") but block is destroyed.",
                __FUNCTION__, image_id);
    }
}

HdrPlusProcessingBlock::GcamFinalImageCallback::GcamFinalImageCallback(
        std::weak_ptr<PipelineBlock> block) : mBlock(block) {
}

void HdrPlusProcessingBlock::GcamFinalImageCallback::Run(int burst_id, gcam::YuvImage* yuv_result,
            gcam::InterleavedImageU8* rgb_result, gcam::GcamPixelFormat pixel_format) const {
    ALOGV("%s: Gcam sent a final image for request %d", __FUNCTION__, burst_id);
    auto block = std::static_pointer_cast<HdrPlusProcessingBlock>(mBlock.lock());
    if (block != nullptr) {
        block->onGcamFinalImage(burst_id, yuv_result, rgb_result, pixel_format);
    } else {
        ALOGE("%s: Gcam sent a final image for request %d but block is destroyed.",
                __FUNCTION__, burst_id);
    }

    if (yuv_result != nullptr) delete yuv_result;
    if (rgb_result != nullptr) delete rgb_result;
}

} // pbcamera