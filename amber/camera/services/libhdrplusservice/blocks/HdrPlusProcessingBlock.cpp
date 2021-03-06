//#define LOG_NDEBUG 0
#define LOG_TAG "HdrPlusProcessingBlock"
#include <log/log.h>

#define ENABLE_HDRPLUS_PROFILER 1

#include <inttypes.h>
#include <stdlib.h>
#include <system/graphics.h>

#include <easelcontrol.h>

#include "googlex/gcam/image/yuv_utils.h"
#include "googlex/gcam/image_io/jpg_helper.h"
#include "googlex/gcam/image_proc/resample.h"
#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/imx_runtime_apis.h"
#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/ipu_algorithms_public.h"
#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/shot_interface.h"

#include "HdrPlusProcessingBlock.h"
#include "HdrPlusPipeline.h"

namespace pbcamera {

namespace {
// Atrace event starting from HDR+ beginning to base frame available
const char* kBaseFrame = "HDR+ baseframe";
// Atrace event starting from base frame available to final image (yuv) finishes
const char* kFinalImage = "HDR+ finalimage";
// Atrace event for the final multiple output resample.
const char* kResample = "HDR+ resample";

std::once_flag loadPcgOnce;
}  // namespace


std::atomic<bool> gPcgLoaded(false);

HdrPlusProcessingBlock::HdrPlusProcessingBlock(std::weak_ptr<SourceCaptureBlock> sourceCaptureBlock,
        bool skipTimestampCheck, int32_t cameraId,
        ImxMemoryAllocatorHandle imxMemoryAllocatorHandle,
        std::shared_ptr<MessengerToHdrPlusClient> messenger) :
        PipelineBlock("HdrPlusProcessingBlock"),
        mMessengerToClient(messenger),
        mSourceCaptureBlock(sourceCaptureBlock),
        mSkipTimestampCheck(skipTimestampCheck),
        mCameraId(cameraId),
        mImxMemoryAllocatorHandle(imxMemoryAllocatorHandle) {
}

HdrPlusProcessingBlock::~HdrPlusProcessingBlock() {
    if (!mInputIdMap.empty()) {
        ALOGE("%s: Some input buffers are still referenced!", __FUNCTION__);
    }
    if (mLoadPcgThread.joinable()) mLoadPcgThread.join();
}

std::shared_ptr<HdrPlusProcessingBlock> HdrPlusProcessingBlock::newHdrPlusProcessingBlock(
        std::weak_ptr<HdrPlusPipeline> pipeline, std::shared_ptr<StaticMetadata> metadata,
        std::weak_ptr<SourceCaptureBlock> sourceCaptureBlock, bool skipTimestampCheck,
        int32_t cameraId, ImxMemoryAllocatorHandle imxMemoryAllocatorHandle,
        std::shared_ptr<MessengerToHdrPlusClient> messenger) {
    ALOGV("%s", __FUNCTION__);

    auto block = std::shared_ptr<HdrPlusProcessingBlock>(
            new HdrPlusProcessingBlock(sourceCaptureBlock, skipTimestampCheck, cameraId,
                    imxMemoryAllocatorHandle, messenger));
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

bool HdrPlusProcessingBlock::isReady() {
    {
        std::unique_lock<std::mutex> lock(mHdrPlusProcessingLock);
        if (mGcam == nullptr) {
            ALOGW("%s: GCAM is not initialized yet.", __FUNCTION__);
            return false;
        }

        if (mPendingShotCapture != nullptr) {
            ALOGW("%s: HDR+ shot pending", __FUNCTION__);
            return false;
        }
    }

    if (EaselControlServer::getThermalCondition() >=
        EaselControlServer::ThermalCondition::Critical) {
        ALOGW("Easel too hot");
        return false;
    }

    {
        std::unique_lock<std::mutex> lock(mQueueLock);

        auto pipeline = mPipeline.lock();
        if (pipeline == nullptr) {
            ALOGE("%s: Pipeline is destroyed.", __FUNCTION__);
            return false;
        }

        checkOldInputsLocked(pipeline, /*returnOldInputs*/true);

        if (mInputQueue.size() < kGcamMinPayloadFrames) {
            ALOGW("%s: Not enough input buffers: %zu", __FUNCTION__, mInputQueue.size());
            return false;
        } else if (mOutputRequestQueue.size() > 0) {
            ALOGW("%s: There is a pending output request.", __FUNCTION__);
            return false;
        }
    }

    if (!gPcgLoaded) return false;

    return true;
}

void HdrPlusProcessingBlock::returnInputLocked(const std::shared_ptr<HdrPlusPipeline> &pipeline,
        Input *input) {
    if (pipeline == nullptr || input == nullptr) return;

    // Unlock the frame buffer before returning it.
    for (auto & buffer : input->buffers) {
        buffer->unlockData();
    }

    pipeline->inputDone(*input);
}

void HdrPlusProcessingBlock::checkOldInputsLocked(
        const std::shared_ptr<HdrPlusPipeline> &pipeline, bool returnOldInputs) {
    int64_t now;
    status_t res = EaselControlServer::getApSynchronizedClockBoottime(&now);
    if (res != 0) {
        ALOGE("%s: Getting AP synchronized clock boot time failed.", __FUNCTION__);
        return;
    }

    // Remove old inputs
    if (!mSkipTimestampCheck) {
        auto input = mInputQueue.begin();
        while (input != mInputQueue.end()) {
            if (now - input->metadata.frameMetadata->easelTimestamp > kOldInputTimeThresholdNs) {
                if (returnOldInputs) {
                    ALOGI("%s: Return an old input with time %" PRId64 " now %" PRId64, __FUNCTION__,
                            input->metadata.frameMetadata->easelTimestamp, now);
                    returnInputLocked(pipeline, &*input);
                    input = mInputQueue.erase(input);
                } else {
                    ALOGW("%s: Found an old input with time %" PRId64 " now %" PRId64, __FUNCTION__,
                            input->metadata.frameMetadata->easelTimestamp, now);
                    input++;
                }
            } else {
                input++;
            }
        }
    }
}

bool HdrPlusProcessingBlock::doWorkLocked() {
    ALOGV("%s", __FUNCTION__);

    std::call_once(loadPcgOnce, [&] {
        mLoadPcgThread = std::thread([&] {
            gcam::LoadPrecompiledGraphs();
            gPcgLoaded = true;
        });
    });

    std::vector<Input> inputs;
    OutputRequest outputRequest = {};

    // Notify shutters and postviews that are ready.
    notifyShuttersAndPostviews();

    std::unique_lock<std::mutex> lock(mHdrPlusProcessingLock);

    // Initialize Gcam if not yet.
    if (mGcam == nullptr) {
        status_t res = initGcam();
        if (res != 0) {
            ALOGE("%s: Initializing Gcam failed: %s (%d).", __FUNCTION__, strerror(-res), res);
            return false;
        }
    }

    // Check if there is a pending Gcam shot capture.
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

        checkOldInputsLocked(pipeline, /*returnOldInputs*/false);

        // If we have more inputs than we need, remove the oldest ones.
        while (mInputQueue.size() > kGcamMaxZslFrames) {
            ALOGV("%s: Input queue is full (%zu). Send the oldest buffer back.", __FUNCTION__,
                    mInputQueue.size());

            returnInputLocked(pipeline, &mInputQueue[0]);
            mInputQueue.pop_front();
        }

        if (mInputQueue.size() < kGcamMinPayloadFrames) {
            // Nothing to do this time.
            ALOGW("%s: Not enough inputs (%zu but need %d).", __FUNCTION__, mInputQueue.size(),
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

    if (mLoadPcgThread.joinable()) mLoadPcgThread.join();

    status_t res = handleCaptureRequestLocked(inputs, outputRequest);
    if (res != 0) {
        ALOGE("%s: Handling capture request failed: %s (%d).", __FUNCTION__, strerror(-res), res);

        // Push inputs and output request back to the front of the queue.
        std::unique_lock<std::mutex> lock(mQueueLock);
        mInputQueue.insert(mInputQueue.begin(), inputs.begin(), inputs.end());
        mOutputRequestQueue.push_front(outputRequest);

        return false;
    }

    return true;
}

status_t HdrPlusProcessingBlock::flushLocked() {
    // Wait until there is no pending shot.
    std::unique_lock<std::mutex> eventLock(mHdrPlusProcessingLock);
    mShotCompletedCondition.wait(eventLock, [&] { return mPendingShotCapture == nullptr; });
    return 0;
}

status_t HdrPlusProcessingBlock::calculateCropRect(int32_t inputCropW, int32_t inputCropH,
    int32_t outputW, int32_t outputH, float *outputCropX0, float *outputCropY0,
    float *outputCropX1, float *outputCropY1) {

    if (outputCropX0 == nullptr || outputCropY0 == nullptr || outputCropX1 == nullptr ||
        outputCropY1 == nullptr) {
        return -EINVAL;
    }

    float x, y, w, h;
    if (inputCropW * outputH > outputW * inputCropH) {
        // If the input crop aspect ratio is larger than output aspect ratio.
        h = inputCropH;
        y = 0;
        w = outputW * h / outputH;
        x = (inputCropW - w) / 2;
    } else {
        // If the input crop aspect ratio is smaller than or equal to output aspect ratio.
        w = inputCropW;
        x = 0;
        h = outputH * w / outputW;
        y = (inputCropH - h) / 2;
    }

    *outputCropX0 = x;
    *outputCropY0 = y;
    *outputCropX1 = x + w;
    *outputCropY1 = y + h;

    return 0;
}

status_t HdrPlusProcessingBlock::fillGcamShotParams(gcam::ShotParams *shotParams,
        gcam::GcamPixelFormat *outputFormat, const std::vector<Input> &inputs,
        const OutputRequest &outputRequest) {
    if (shotParams == nullptr) {
        ALOGE("%s: shotParams is nullptr.", __FUNCTION__);
        return -EINVAL;
    }

    if (outputFormat == nullptr) {
        ALOGE("%s: outputFormat is nullptr.", __FUNCTION__);
        return -EINVAL;
    }

    int32_t zoomCropX, zoomCropY, zoomCropW, zoomCropH;

    zoomCropX = outputRequest.metadata.requestMetadata->cropRegion[0];
    zoomCropY = outputRequest.metadata.requestMetadata->cropRegion[1];
    zoomCropW = outputRequest.metadata.requestMetadata->cropRegion[2];
    zoomCropH = outputRequest.metadata.requestMetadata->cropRegion[3];

    status_t res = 0;

    // Find the largest crop region within the digital zoom crop to fit all output buffer aspect
    // ratios.
    float cropX0 = zoomCropW, cropY0 = zoomCropH, cropX1 = 0, cropY1 = 0;
    for (auto buffer : outputRequest.buffers) {
        switch (buffer->getFormat()) {
            case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            case HAL_PIXEL_FORMAT_YCbCr_420_SP:
            {
                float x0 = 0, y0 = 0, x1 = 0, y1 = 0;

                res = calculateCropRect(zoomCropW, zoomCropH, buffer->getWidth(),
                        buffer->getHeight(), &x0, &y0, &x1, &y1);
                if (res != 0) {
                    ALOGE("%s: Calculating crop rect failed: %s (%d).", __FUNCTION__,
                            strerror(-res), res);
                    return res;
                }

                cropX0 = std::min(cropX0, x0);
                cropY0 = std::min(cropY0, y0);
                cropX1 = std::max(cropX1, x1);
                cropY1 = std::max(cropY1, y1);
                break;
            }
            case HAL_PIXEL_FORMAT_RAW16:
                // RAW16 output will be uncropped.
                break;
            default:
                ALOGE("%s: Output format %d is not supported.", __FUNCTION__, buffer->getFormat());
                return -EINVAL;
        }
    }

    // Gcam target resolution should have the same aspect ratio as the largest crop region's aspect
    // ratio. Find the largest target resolution among all output buffers to avoid upscaling from
    // target resolution to output buffer resolution.
    float cropW = cropX1 - cropX0;
    float cropH = cropY1 - cropY0;
    int32_t maxTargetW = 0, maxTargetH = 0;
    int32_t maxTargetFormat;

    for (auto buffer : outputRequest.buffers) {
        int targetW = 0, targetH = 0;

        // For each output buffer, find the target resolution that matches crop aspect ratio.
        if (cropW * buffer->getHeight() > buffer->getWidth() * cropH) {
            targetH = buffer->getHeight();
            targetW = targetH * cropW / cropH;
        } else {
            targetW = buffer->getWidth();
            targetH = targetW * cropH / cropW;
        }

        if (maxTargetW < targetW) {
            maxTargetW = targetW;
            maxTargetH = targetH;
            maxTargetFormat = buffer->getFormat();
        }
    }

    // Make sure target width and height are even numbers.
    maxTargetW = ((maxTargetW + 1) / 2) * 2;
    maxTargetH = ((maxTargetH + 1) / 2) * 2;

    // Clamp target resolution to active array size.
    maxTargetW = std::min(maxTargetW, mStaticMetadata->activeArraySize[2]);
    maxTargetH = std::min(maxTargetH, mStaticMetadata->activeArraySize[3]);

    // If final crop region is just slightly bigger than target resolution, try to crop more to
    // avoid scaling. This is going to change FOV slightly for better quality and faster processing.
    if (cropW > maxTargetW && cropH > maxTargetH &&
            cropW - maxTargetW < kCropRatioThreshold * maxTargetW &&
            cropH - maxTargetH < kCropRatioThreshold * maxTargetH) {
        cropX0 += (cropW - maxTargetW) / 2;
        cropY0 += (cropH - maxTargetH) / 2;
        cropW = maxTargetW;
        cropH = maxTargetH;
        cropX1 = cropX0 + cropW;
        cropY1 = cropY0 + cropH;
    }

    // Convert crop coordinates to be w.r.t. active array.
    cropX0 += zoomCropX;
    cropX1 += zoomCropX;
    cropY0 += zoomCropY;
    cropY1 += zoomCropY;

    int32_t inputBufferW = inputs[0].buffers[0]->getWidth();
    int32_t inputBufferH = inputs[0].buffers[0]->getHeight();

    if (inputBufferW == mStaticMetadata->pixelArraySize[0] &&
            inputBufferH == mStaticMetadata->pixelArraySize[1]) {
        // If the input buffer resolution is the same as pixel array size, sensor crop is not
        // applied. Normalize the crop region to active array.
        cropX0 /= mStaticMetadata->activeArraySize[2];
        cropX1 /= mStaticMetadata->activeArraySize[2];
        cropY0 /= mStaticMetadata->activeArraySize[3];
        cropY1 /= mStaticMetadata->activeArraySize[3];
    } else {
        // If the input buffer resolution is not the same as pixel array size, sensor crop is
        // applied to the input buffer. Normalize the crop region to input buffer size.
        float inputX0 = (mStaticMetadata->activeArraySize[2] - inputBufferW) / 2;
        float inputY0 = (mStaticMetadata->activeArraySize[3] - inputBufferH) / 2;
        cropX0 = (cropX0 - inputX0) / inputBufferW;
        cropX1 = (cropX1 - inputX0) / inputBufferW;
        cropY0 = (cropY0 - inputY0) / inputBufferH;
        cropY1 = (cropY1 - inputY0) / inputBufferH;
    }

    cropX0 = std::max(cropX0, 0.0f);
    cropX1 = std::min(cropX1, 1.0f);
    cropY0 = std::max(cropY0, 0.0f);
    cropY1 = std::min(cropY1, 1.0f);

    // Clamp AE compensation within a valid range.
    int32_t expCompensation = outputRequest.metadata.requestMetadata->aeExposureCompensation;
    expCompensation = std::max(mStaticMetadata->aeCompensationRange[0], expCompensation);
    expCompensation = std::min(mStaticMetadata->aeCompensationRange[1], expCompensation);

    shotParams->Clear();
    shotParams->ae.target_width = maxTargetW;
    shotParams->ae.target_height = maxTargetH;
    shotParams->ae.crop.x0 = cropX0;
    shotParams->ae.crop.x1 = cropX1;
    shotParams->ae.crop.y0 = cropY0;
    shotParams->ae.crop.y1 = cropY1;
    shotParams->ae.payload_frame_orig_width = inputs[0].buffers[0]->getWidth();
    shotParams->ae.payload_frame_orig_height = inputs[0].buffers[0]->getHeight();
    shotParams->ae.exposure_compensation = mStaticMetadata->aeCompensationStep * expCompensation;
    shotParams->zsl = true;
    shotParams->resampling_method_override = gcam::ResamplingMethod::kRaisr;

    if (mStaticMetadata->flashInfoAvailable == ANDROID_FLASH_INFO_AVAILABLE_FALSE) {
        shotParams->flash_mode = gcam::FlashMode::kOff;
    }

    *outputFormat = maxTargetFormat == HAL_PIXEL_FORMAT_YCrCb_420_SP ?
            gcam::GcamPixelFormat::kNv21 : gcam::GcamPixelFormat::kNv12;

    return 0;
}

gcam::ShotCallbacks HdrPlusProcessingBlock::getShotCallbacks(bool isPostviewEnabled) {
    gcam::ShotCallbacks shotCallbacks = {
            /*error_callback*/nullptr,
            /*base_frame_callback*/mGcamBaseFrameCallback.get(),
            /*postview_callback*/isPostviewEnabled ? mGcamPostviewCallback.get() : nullptr,
            /*merge_raw_image_callback*/nullptr,
            /*merged_pd_callback*/nullptr,
            /*merged_dng_callback*/nullptr,
            /*final_image_callback*/mGcamFinalImageCallback.get(),
            /*jpeg_callback*/nullptr,
            /*progress_callback*/nullptr,
            /*finished_callback*/nullptr};

    return shotCallbacks;
}

void HdrPlusProcessingBlock::fillGcamImageSaverParams(gcam::ImageSaverParams *param) {
    if (param == nullptr) {
        ALOGE("%s: param is nullptr.", __FUNCTION__);
        return;
    }

    // Make the path in the format of "gcam_<current_ap_timestamp>"
    std::string destFolder = "gcam_";
    int64_t now;
    if (EaselControlServer::getApSynchronizedClockBoottime(&now) == 0) {
        destFolder.append(std::to_string(now));
    }

    if (destFolder.size() > kGcamMaxFilenameLength) {
        destFolder.erase(kGcamMaxFilenameLength - 1);
    }

    param->dest_folder = destFolder;
}

status_t HdrPlusProcessingBlock::handleCaptureRequestLocked(const std::vector<Input> &inputs,
        const OutputRequest &outputRequest) {
    std::shared_ptr<SourceCaptureBlock> sourceCaptureBlock;
    bool continuousCapturing = outputRequest.metadata.requestMetadata->continuousCapturing;

    sourceCaptureBlock = mSourceCaptureBlock.lock();
    if (sourceCaptureBlock != nullptr) {
        sourceCaptureBlock->notifyIpuProcessingStart(continuousCapturing);
    }

    auto shotCapture = std::make_shared<ShotCapture>();

    // Start a HDR+ shot.
    status_t res = IssueShotCapture(shotCapture, inputs, outputRequest);
    if (res != 0) {
        ALOGE("%s: Issuing a HDR+ capture failed: %s (%d).", __FUNCTION__, strerror(-res), res);
        sourceCaptureBlock->notifyIpuProcessingDone();

        return res;
    }

    shotCapture->outputRequest = outputRequest;
    shotCapture->baseFrameIndex = kInvalidBaseFrameIndex;
    mPendingShotCapture = shotCapture;
    return 0;
}

status_t HdrPlusProcessingBlock::IssueShotCapture(std::shared_ptr<ShotCapture> shotCapture,
        const std::vector<Input> &inputs, const OutputRequest &outputRequest) {
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
    gcam::GcamPixelFormat outputFormat;
    status_t res = fillGcamShotParams(&shotParams, &outputFormat, inputs, outputRequest);
    if (res != 0) {
        ALOGE("%s: Failed to decide output resolution: %s (%d).", __FUNCTION__, strerror(-res),
                res);
        return res;
    }

    gcam::ImageSaverParams imageSaverParams;
    fillGcamImageSaverParams(&imageSaverParams);

    START_PROFILER_TIMER(shotCapture->timer);

    gcam::PostviewParams postviewParams;
    postviewParams.pixel_format = kGcamPostviewFormat;
    postviewParams.target_width = mCameraId == 0 ? kGcamPostviewWidthBack : kGcamPostviewWidthFront;
    // Don't speficy target_height for libgcam to decide.

    gcam::ShotCallbacks shotCallbacks = getShotCallbacks(
            outputRequest.metadata.requestMetadata->postviewEnable);

    // camera_id is always 0 because we only set 1 static metadata in GCAM for current camera
    // which could be rear or front camera.
    gcam::IShot* shot = mGcam->StartShotCapture(
            /*camera_id*/0,
            shotParams,
            shotCallbacks,
            outputFormat,
            /*final_yuv_id=*/gcam::kInvalidImageId,
            /*final_output_yuv_view=*/gcam::YuvWriteView(),
            /*final_rgb_id=*/gcam::kInvalidImageId,
            /*final_output_rgb_view=*/gcam::InterleavedWriteViewU8(),
            /*merged_raw_id=*/gcam::kInvalidImageId,
            /*merged_raw_view=*/gcam::RawWriteView(),
            postviewParams,
            /*image_saver_params*/&imageSaverParams);
    if (shot == nullptr) {
        ALOGE("%s: Failed to start a shot capture.", __FUNCTION__);
        return -ENODEV;
    }

    shotCapture->shotId = shot->shot_id();
    mMessengerToClient->notifyAtraceAsync(kBaseFrame, shotCapture->shotId, kAtraceBegin);

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
        mMessengerToClient->notifyAtraceAsync(kBaseFrame, shotCapture->shotId, kAtraceEnd);
        return -ENODEV;
    }

    // End shot capture.
    if (!mGcam->EndShotCapture(shot)) {
        ALOGE("%s: Failed to end a shot capture.", __FUNCTION__);
        mMessengerToClient->notifyAtraceAsync(kBaseFrame, shotCapture->shotId, kAtraceEnd);
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
    res = input.buffers[0]->lockData();
    if (res != 0) {
        ALOGE("%s: Locking buffer data failed: %s (%d)", __FUNCTION__,
                strerror(-res), res);
        return res;
    }

    int64_t imageId = (uintptr_t)input.buffers[0]->getPlaneData(0);
    gcam::RawWriteView raw(input.buffers[0]->getWidth(), input.buffers[0]->getHeight(),
            input.buffers[0]->getStride(0) - widthBytes, layout, input.buffers[0]->getPlaneData(0));

    // Create unused phase detect data.
    gcam::InterleavedWriteViewU16 pd_view;
    int64_t pd_id = gcam::kInvalidImageId;

    if (!shot->AddPayloadFrame(frame->gcamFrameMetadata,
            imageId, raw, pd_id, pd_view, *frame->gcamSpatialGainMap.get())) {
        ALOGE("%s: Adding a payload frame failed.", __FUNCTION__);
        return -ENODEV;
    }

    frame->input = input;
    addInputReference(imageId, input);

    return 0;
}

void HdrPlusProcessingBlock::notifyShuttersAndPostviews() {
    while (1) {
        Shutter shutter = {};
        {
            std::unique_lock<std::mutex> lock(mShuttersLock);
            if (mShutters.size() == 0) {
                break;
            }
            shutter = mShutters[0];
            mShutters.pop_front();
        }

        notifyShutter(shutter);
    }

    while (1) {
        Postview postview = {};
        {
            std::unique_lock<std::mutex> lock(mPostviewsLock);
            if (mPostviews.size() == 0) {
                break;
            }
            postview = std::move(mPostviews[0]);
            mPostviews.pop_front();
        }
        notifyPostview(postview);
    }
}

void HdrPlusProcessingBlock::notifyShutter(const Shutter &shutter) {
    uint32_t requestId = 0;
    int64_t apSensorTimestampNs = 0;

    {
        std::unique_lock<std::mutex> lock(mHdrPlusProcessingLock);

        if (mPendingShotCapture == nullptr) {
            ALOGE("%s: There is no pending shot for shot id %d. Dropping a base frame index %d.",
                    __FUNCTION__, shutter.shotId, shutter.baseFrameIndex);
            return;
        }

        if (shutter.shotId != mPendingShotCapture->shotId) {
            ALOGE("%s: Expecting a base frame index for shot %d but got a final image for shot %d.",
                    __FUNCTION__, mPendingShotCapture->shotId, shutter.shotId);
            return;
        }

        if (shutter.baseFrameIndex >= static_cast<int>(mPendingShotCapture->frames.size())) {
            ALOGE("%s: baseFrameIndex is %d but there are only %zu frames", __FUNCTION__,
                    shutter.baseFrameIndex, mPendingShotCapture->frames.size());
            return;
        }

        if (mPendingShotCapture->baseFrameIndex != kInvalidBaseFrameIndex) {
            ALOGE("%s: baseFrameIndex is already selected for shot %d", __FUNCTION__, shutter.shotId);
            return;
        }

        mPendingShotCapture->baseFrameIndex = shutter.baseFrameIndex;

        requestId = mPendingShotCapture->outputRequest.metadata.requestId;
        apSensorTimestampNs = mPendingShotCapture->frames[shutter.baseFrameIndex]->
                input.metadata.frameMetadata->timestamp;

    }

    mMessengerToClient->notifyShutterAsync(requestId, apSensorTimestampNs);
}

bool HdrPlusProcessingBlock::isTheSameYuvFormat(gcam::YuvFormat gcamFormat, int halFormat) {
    switch (gcamFormat) {
        case gcam::YuvFormat::kNv12:
            return halFormat == HAL_PIXEL_FORMAT_YCbCr_420_SP;
        case gcam::YuvFormat::kNv21:
            return halFormat == HAL_PIXEL_FORMAT_YCrCb_420_SP;
        default:
            return false;
    }
}

status_t HdrPlusProcessingBlock::copyBuffer(const std::unique_ptr<gcam::YuvImage> &srcYuvImage,
        PipelineBuffer *dstBuffer) {
    if (srcYuvImage == nullptr || dstBuffer == nullptr) {
        ALOGE("%s: srcYuvImage (%p) or dstBuffer (%p) is nullptr.", __FUNCTION__, srcYuvImage.get(),
                dstBuffer);
        return -EINVAL;
    }

    if (!isTheSameYuvFormat(srcYuvImage->yuv_format(), dstBuffer->getFormat())) {
        ALOGE("%s: Src image format is %s but dst buffer format is %d.", __FUNCTION__,
                gcam::ToText(srcYuvImage->yuv_format()), dstBuffer->getFormat());
        return -EINVAL;
    }

    dstBuffer->lockData();
    uint8_t *lumaDst = dstBuffer->getPlaneData(0);

    // Copy luma line by line from the final image.
    const gcam::InterleavedReadViewU8 &lumaImageSrc = srcYuvImage->luma_read_view();
    int32_t lineBytesToCopy = std::min(dstBuffer->getWidth(), lumaImageSrc.width());
    uint32_t linesToCopy = std::min(dstBuffer->getHeight(), lumaImageSrc.height());
    for (uint32_t y = 0; y < linesToCopy; y++) {
        std::memcpy(lumaDst + y * dstBuffer->getStride(0), &lumaImageSrc.at(0, y, 0),
                lineBytesToCopy);
    }

    // Copy chroma line by line from the final image.
    const gcam::InterleavedReadViewU8 &chromaImageSrc = srcYuvImage->chroma_read_view();
    uint8_t* chromaDst = dstBuffer->getPlaneData(1);
    lineBytesToCopy = std::min(dstBuffer->getWidth() , chromaImageSrc.width() * 2);
    linesToCopy = std::min(dstBuffer->getHeight() / 2, chromaImageSrc.height());
    for (uint32_t y = 0; y < linesToCopy; y++) {
        std::memcpy(chromaDst + y * dstBuffer->getStride(1), &chromaImageSrc.at(0, y, 0),
                lineBytesToCopy);
    }

    dstBuffer->unlockData();

    return 0;
}

status_t HdrPlusProcessingBlock::resampleBuffer(const std::unique_ptr<gcam::YuvImage> &srcYuvImage,
        PipelineBuffer *dstBuffer) {
    if (srcYuvImage == nullptr || dstBuffer == nullptr) {
        ALOGE("%s: srcYuvImage (%p) or dstBuffer (%p) is nullptr.", __FUNCTION__, srcYuvImage.get(),
                dstBuffer);
        return -EINVAL;
    }

    ALOGV("%s: Resampling from %dx%d to %dx%d", __FUNCTION__, srcYuvImage->luma_read_view().width(),
        srcYuvImage->luma_read_view().height(), dstBuffer->getWidth(), dstBuffer->getHeight());

    // Logically crop source YUV image to match dstBuffer aspect ration.
    float cropX0, cropY0, cropX1, cropY1;
    status_t res = calculateCropRect(srcYuvImage->luma_read_view().width(),
            srcYuvImage->luma_read_view().height(), dstBuffer->getWidth(), dstBuffer->getHeight(),
            &cropX0, &cropY0, &cropX1, &cropY1);

    gcam::YuvReadView croppedSrcYuvImage(*srcYuvImage);

    // Snap the cropping to even number to avoid dimension overflow.
    int cropX0Int, cropY0Int, cropX1Int, cropY1Int;
    cropX0Int = static_cast<int>(cropX0) & ~1;
    cropX1Int = static_cast<int>(std::ceil(cropX1) + 1) & ~1;
    cropY0Int = static_cast<int>(cropY0) & ~1;
    cropY1Int = static_cast<int>(std::ceil(cropY1) + 1) & ~1;

    croppedSrcYuvImage.FastCrop(cropX0Int, cropY0Int, cropX1Int, cropY1Int);

    int32_t format = dstBuffer->getFormat();
    gcam::YuvFormat gcamYuvFormat;
    switch (format) {
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            gcamYuvFormat = gcam::YuvFormat::kNv21;
            break;
        case HAL_PIXEL_FORMAT_YCbCr_420_SP:
            gcamYuvFormat = gcam::YuvFormat::kNv12;
            break;
        default:
            ALOGE("%s: dstBuffer format %d is not supported.", __FUNCTION__, format);
            return -EINVAL;
    }

    dstBuffer->lockData();

    gcam::YuvWriteView dstYuvImage(dstBuffer->getWidth(), dstBuffer->getHeight(),
            /*luma_channels*/1,
           dstBuffer->getStride(0), dstBuffer->getPlaneData(0),
           dstBuffer->getWidth() / 2, dstBuffer->getHeight() / 2, /*chroma_channels*/2,
           dstBuffer->getStride(1), dstBuffer->getPlaneData(1),
           gcamYuvFormat);

    bool success = gcam::ResampleIpu(croppedSrcYuvImage, &dstYuvImage, /*copy_to_device*/true);

    if (!success) {
        ALOGE("%s: Converting destination RGB image to YUV failed.", __FUNCTION__);
        res = -ENODEV;
    }

    dstBuffer->unlockData();

    return res;
}

status_t HdrPlusProcessingBlock::produceRequestOutputBuffers(
        std::unique_ptr<gcam::YuvImage> srcYuvImage, PipelineBufferSet *outputBuffers) {
    if (srcYuvImage == nullptr || outputBuffers == nullptr) {
        ALOGE("%s: srcYuvImage (%p) or outputBuffers (%p) is nullptr.", __FUNCTION__,
                srcYuvImage.get(), outputBuffers);
        return -EINVAL;
    }

    status_t res;
    PipelineBuffer* bufferToAttach = nullptr;

    for (auto outputBuffer : *outputBuffers) {
        if (srcYuvImage->luma_read_view().width() == outputBuffer->getWidth() &&
                srcYuvImage->luma_read_view().height() == outputBuffer->getHeight() &&
                isTheSameYuvFormat(srcYuvImage->yuv_format(), outputBuffer->getFormat())) {
            if (bufferToAttach == nullptr && outputBuffer->attachable(srcYuvImage)) {
                bufferToAttach = outputBuffer;
            } else {
                // If the image cannot be attached, allocate the output buffer and copy the image
                // content to the buffer.
                res = ((PipelineImxBuffer*)outputBuffer)->allocate(mImxMemoryAllocatorHandle);
                if (res != 0) {
                    ALOGE("%s: Allocating buffer failed: %s (%d).", __FUNCTION__, strerror(-res), res);
                    return res;
                }

                res = copyBuffer(srcYuvImage, outputBuffer);
                if (res != 0) {
                    ALOGE("%s: Copying buffer failed: %s (%d).", __FUNCTION__, strerror(-res), res);
                    return res;
                }
            }
        } else {
            // Allocate the output buffer for resampling.
            res = ((PipelineImxBuffer*)outputBuffer)->allocate(mImxMemoryAllocatorHandle);
            if (res != 0) {
                ALOGE("%s: Allocating buffer failed: %s (%d).", __FUNCTION__, strerror(-res), res);
                return res;
            }

            res = resampleBuffer(srcYuvImage, outputBuffer);
            if (res != 0) {
                ALOGE("%s: Resampling buffer failed: %s (%d).", __FUNCTION__, strerror(-res), res);
                return res;
            }
        }
    }

    if (bufferToAttach != nullptr) {
        res = bufferToAttach->attachImage(&srcYuvImage);
        if (res != 0) {
            ALOGE("%s: Attaching image to buffer failed: %s (%d).", __FUNCTION__, strerror(-res),
                    res);
            return res;
        }
    }

    return 0;
}

void HdrPlusProcessingBlock::onGcamBaseFrameCallback(int shotId, int baseFrameIndex, int64_t baseFrameTimestampNs) {
    ALOGD("%s: Gcam selected a base frame index %d for shot %d.", __FUNCTION__, baseFrameIndex,
        shotId);
    mMessengerToClient->notifyAtraceAsync(kBaseFrame, shotId, kAtraceEnd);

    {
        std::unique_lock<std::mutex> lock(mShuttersLock);
        Shutter shutter = {};
        shutter.shotId = shotId;
        shutter.baseFrameIndex = baseFrameIndex;
        shutter.baseFrameTimestampNs = baseFrameTimestampNs;
        mShutters.push_back(shutter);
    }

    mMessengerToClient->notifyAtraceAsync(kFinalImage, shotId, kAtraceBegin);

    // Notify worker thread.
    notifyWorkerThreadEvent();
}

void HdrPlusProcessingBlock::notifyPostview(const Postview &postview) {
    if (mPendingShotCapture == nullptr) {
        ALOGE("%s: There is no pending shot for shot id %d. Dropping a postview.",
                __FUNCTION__, postview.shotId);
        return;
    }

    if (postview.shotId != mPendingShotCapture->shotId) {
        ALOGE("%s: Expecting a postview for shot %d but got a postview for shot %d.",
                __FUNCTION__, mPendingShotCapture->shotId, postview.shotId);
        return;
    }

    if (postview.rgbImage == nullptr) {
        ALOGE("%s: Postview for shot %d is nullptr.", __FUNCTION__, postview.shotId);
        return;
    }

    mMessengerToClient->notifyPostview(mPendingShotCapture->outputRequest.metadata.requestId,
            postview.rgbImage->base_pointer(), /*fd*/-1, postview.rgbImage->width(),
            postview.rgbImage->height(), postview.rgbImage->y_stride(), HAL_PIXEL_FORMAT_RGB_888);
}

void HdrPlusProcessingBlock::onGcamPostview(int32_t shotId,
        std::unique_ptr<gcam::YuvImage> yuvResult,
        std::unique_ptr<gcam::InterleavedImageU8> rgbResult, gcam::GcamPixelFormat pixelFormat) {
    ALOGI("%s: Got a postview for shot %d from GCAM", __FUNCTION__, shotId);

    if (yuvResult != nullptr) {
        ALOGE("%s: Not expecting a YUV postview.", __FUNCTION__);
        return;
    }

    if (rgbResult == nullptr) {
        ALOGW("%s: Expecting an RGB postview from GCAM but rgbResult is nullptr.", __FUNCTION__);
        return;
    }

    if (pixelFormat != gcam::GcamPixelFormat::kRgb) {
        ALOGE("%s: Expecting RGB but got format %d. Dropping this result.", __FUNCTION__,
                pixelFormat);
        return;
    }

    {
        std::unique_lock<std::mutex> lock(mPostviewsLock);

        Postview postview = {};
        postview.shotId = shotId;
        postview.rgbImage = std::move(rgbResult);
        mPostviews.push_back(std::move(postview));
    }

    // Notify worker thread.
    notifyWorkerThreadEvent();
}

void HdrPlusProcessingBlock::onGcamInputImageReleased(const int64_t imageId) {
    ALOGD("%s: Got image %" PRId64, __FUNCTION__, imageId);
    removeInputReference(imageId);
}

void HdrPlusProcessingBlock::onGcamFinalImage(int shotId, std::unique_ptr<gcam::YuvImage> yuvResult,
        gcam::GcamPixelFormat pixelFormat, const gcam::ExifMetadata& exifMetadata) {
    ALOGD("%s: Got a final image (format %d) for request %d.", __FUNCTION__, pixelFormat, shotId);
    mMessengerToClient->notifyAtraceAsync(kFinalImage, shotId, kAtraceEnd);

    if (yuvResult == nullptr) {
        ALOGE("%s: Expecting a YUV final image but yuvResult is nullptr.", __FUNCTION__);
        return;
    }

    std::shared_ptr<ShotCapture> finishingShot;
    {
        std::unique_lock<std::mutex> lock(mHdrPlusProcessingLock);

        if (mPendingShotCapture == nullptr) {
            ALOGE("%s: There is no pending shot for shot id %d. Dropping a final image.",
                    __FUNCTION__, shotId);
            return;
        }

        if (shotId != mPendingShotCapture->shotId) {
            ALOGE("%s: Expecting a final image for shot %d but got a final image for shot %d.",
                    __FUNCTION__, mPendingShotCapture->shotId, shotId);
            return;
        }

        finishingShot = mPendingShotCapture;
        mPendingShotCapture = nullptr;
    }

    OutputResult outputResult = finishingShot->outputRequest;

    // Notify AP that it's ready to take another capture request.
    mMessengerToClient->notifyNextCaptureReadyAsync(outputResult.metadata.requestId);

    mMessengerToClient->notifyAtraceAsync(kResample, shotId, kAtraceBegin);
    status_t res = produceRequestOutputBuffers(std::move(yuvResult), &outputResult.buffers);
    mMessengerToClient->notifyAtraceAsync(kResample, shotId, kAtraceEnd);

    END_PROFILER_TIMER(finishingShot->timer);

    if (res != 0) {
        ALOGE("%s: Producing request output buffers failed: %s (%d).", __FUNCTION__, strerror(-res),
                res);
        for (auto buffer : outputResult.buffers) {
            buffer->destroy();
        }
        return;
    }

    auto sourceCaptureBlock = mSourceCaptureBlock.lock();
    if (sourceCaptureBlock != nullptr) {
        sourceCaptureBlock->notifyIpuProcessingDone();
    }

    // Set frame metadata.
    outputResult.metadata.frameMetadata =
            finishingShot->frames[finishingShot->baseFrameIndex]->input.metadata.frameMetadata;

    // Set the result metadata. GCAM should provide more result metadata. b/32721233.
    outputResult.metadata.resultMetadata = std::make_shared<ResultMetadata>();
    outputResult.metadata.resultMetadata->easelTimestamp =
            outputResult.metadata.frameMetadata->easelTimestamp;
    outputResult.metadata.resultMetadata->timestamp =
            outputResult.metadata.frameMetadata->timestamp;

    int makernoteSize = 0;
    outputResult.metadata.resultMetadata->makernote.resize(gcam::kMaxMakernoteSize);
    gcam::EncodeMakerNote(exifMetadata.makernote.c_str(),
            &outputResult.metadata.resultMetadata->makernote[0], &makernoteSize);
    outputResult.metadata.resultMetadata->makernote.erase(makernoteSize);

    auto pipeline = mPipeline.lock();
    if (pipeline != nullptr) {
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
    } else {
        ALOGW("%s: Pipeline is destroyed.", __FUNCTION__);
    }

    // Notify worker thread that it can start next processing.
    notifyWorkerThreadEvent();

    // Notify shot is completed.
    mShotCompletedCondition.notify_one();
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

    gcam::DngColorCalibration colorCalibration[2];
    colorCalibration[0].illuminant =
            static_cast<gcam::DngColorCalibration::Illuminant>(metadata->referenceIlluminant1);
    colorCalibration[1].illuminant =
            static_cast<gcam::DngColorCalibration::Illuminant>(metadata->referenceIlluminant2);
    for (uint32_t i = 0; i < 9; i++) {
        colorCalibration[0].xyz_to_model_rgb[i] = metadata->colorTransform1[i];
        colorCalibration[0].model_rgb_to_device_rgb[i] = metadata->calibrationTransform1[i];
        colorCalibration[1].xyz_to_model_rgb[i] = metadata->colorTransform2[i];
        colorCalibration[1].model_rgb_to_device_rgb[i] = metadata->calibrationTransform2[i];
    }

    gcamMetadata->dng_color_calibration.push_back(colorCalibration[0]);
    gcamMetadata->dng_color_calibration.push_back(colorCalibration[1]);
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
    static const char* kGcamDevice = "walleye";
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

    gcamMetadata->post_raw_digital_gain = metadata->postRawSensitivityBoost > 0 ?
            metadata->postRawSensitivityBoost / kPostRawSensitivityBoostUnity : 1.0f;
    gcamMetadata->flash =
            (metadata->flashMode == ANDROID_FLASH_MODE_SINGLE ||
             metadata->flashMode == ANDROID_FLASH_MODE_TORCH) ?
            gcam::FlashMetadata::kOn : gcam::FlashMetadata::kOff;
    gcamMetadata->wb.color_temp = gcam::kColorTempUnknown;

    // Remap Camera2 order {R, G_even, G_odd, B} to Gcam order {R, GR, GB, B}
    uint8_t cfa = mStaticMetadata->colorFilterArrangement;
    for (uint32_t i = 0; i < 4; i++) {
        gcamMetadata->wb.gains[i] =
                metadata->colorCorrectionGains[getCameraChannelIndex(i, cfa)];
    }

    for (uint32_t i = 0; i < 9; i ++) {
        gcamMetadata->wb.rgb2rgb[i] = metadata->colorCorrectionTransform[i];
    }

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
    if (metadata->faceRectangles.size() != metadata->faceScores.size()) {
        ALOGE("%s: The numbers of face information don't match: face rectangles: %zu, "
                "face scores: %zu.", __FUNCTION__, metadata->faceRectangles.size(),
                metadata->faceScores.size());
        return -EINVAL;
    }

    // If face detection mode is full, check the number of face IDs.
    if (metadata->faceDetectMode == ANDROID_STATISTICS_FACE_DETECT_MODE_FULL &&
        metadata->faceIds.size() != metadata->faceRectangles.size()) {
        ALOGE("%s: The numbers of face information don't match: face IDs: %zu, "
                "face rectangles: %zu, face scores: %zu.", __FUNCTION__,
                metadata->faceIds.size(), metadata->faceRectangles.size(),
                metadata->faceScores.size());
        return -EINVAL;
    }

    // Convert face info.
    float arrayW = static_cast<float>(mStaticMetadata->activeArraySize[2]);
    float arrayH = static_cast<float>(mStaticMetadata->activeArraySize[3]);
    for (uint32_t i = 0; i < metadata->faceRectangles.size(); i++) {
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

    frame->gcamSpatialGainMap = std::make_shared<gcam::SpatialGainMap>(smWidth, smHeight,
            /*is_precise*/true, /*has_extra_vignetting_applied*/false);

    if (metadata->lensShadingMap.size() != smHeight * smWidth * 4) {
        ALOGE("%s: Lens shading map has %lu entries. Expecting %u", __FUNCTION__,
                metadata->lensShadingMap.size(), smHeight * smWidth * 4);
        return -EINVAL;
    }

    for (uint32_t c = 0; c < 4; c++) {
        for (uint32_t y = 0; y < smHeight; y++) {
            for (uint32_t x = 0; x < smWidth; x++) {
                uint32_t index = (y * smWidth + x) * 4 + getCameraChannelIndex(c, cfa);
                frame->gcamSpatialGainMap->WriteRggb(x, y, c, metadata->lensShadingMap[index]);
            }
        }
    }

    gcamMetadata->ae.mode = metadata->aeMode;
    gcamMetadata->ae.lock = metadata->aeLock == ANDROID_CONTROL_AE_LOCK_ON;
    gcamMetadata->ae.state = static_cast<gcam::AeState>(metadata->aeState);
    gcamMetadata->ae.precapture_trigger = metadata->aePrecaptureTrigger;
    gcamMetadata->ae.exposure_compensation = metadata->aeExposureCompensation *
        mStaticMetadata->aeCompensationStep;

    for (auto & aeRegion : metadata->aeRegions) {
        gcam::WeightedPixelRect rect = {};
        rect.rect.x0 = aeRegion[0];
        rect.rect.y0 = aeRegion[1];
        rect.rect.x1 = aeRegion[2];
        rect.rect.y1 = aeRegion[3];
        rect.weight = aeRegion[4];

        gcamMetadata->ae.metering_rectangles.push_back(rect);
    }

    return 0;
}

bool HdrPlusProcessingBlock::onGcamFileSaver(const void* data, size_t bytes,
        const std::string& filename) {
    // We might have an Imx buffer, or we might not. If we fail to get an
    // Imx buffer then we always fallback to using the pointer. If we do
    // have an Imx buffer, then we have to handle the case where its
    // backed by malloc.
    ImxDeviceBufferHandle handle = nullptr;
    int fd = -1;
    uint64_t offset = 0;
    ImxError err = ImxGetDeviceBufferFromAddress(data, &handle, &offset);
    if (err == IMX_SUCCESS) {
        ALOGI("%s: Received ion buffer.", __FUNCTION__);
        ImxShareDeviceBuffer(handle, &fd);
        void* dmaData = nullptr;
        if (fd == -1) {
            ALOGI("%s: Allocation made with IMX_MEMORY_ALLOCATOR_MALLOC",
                  __FUNCTION__);
            dmaData = const_cast<void*>(data);
        }
        ALOGI("%s: Got fd=%d for handle=%p addr=%p offset=%lu.",
              __FUNCTION__, fd, handle, data, offset);
        mMessengerToClient->notifyFileDump(filename, dmaData, fd, bytes);
        return true;
    } else {
        ALOGI("%s: Received malloc buffer.", __FUNCTION__);
        mMessengerToClient->notifyFileDump(
            filename, const_cast<void*>(data), /*dmaBufFd=*/-1, bytes);
        return true;
    }
}

status_t HdrPlusProcessingBlock::initGcam() {
    if (mGcamStaticMetadata == nullptr) {
        ALOGE("%s: mGcamStaticMetadata is nullptr.", __FUNCTION__);
        return -ENODEV;
    }

    // Create gcam callbacks.
    mGcamInputImageReleaseCallback =
            std::make_unique<GcamInputImageReleaseCallback>(shared_from_this());
    mGcamFinalImageCallback =
            std::make_unique<GcamFinalImageCallback>(shared_from_this());
    mGcamBaseFrameCallback =
            std::make_unique<GcamBaseFrameCallback>(shared_from_this());
    mGcamPostviewCallback =
            std::make_unique<GcamPostviewCallback>(shared_from_this());
    mGcamFileSaver = std::make_unique<GcamFileSaver>(shared_from_this());

    // Set up gcam init params.
    gcam::InitParams initParams;
    initParams.thread_count = kGcamThreadCounts;
    initParams.tuning_locked = kGcamTuningLocked;
    initParams.use_hexagon = false;
    initParams.max_full_metering_sweep_frames = kGcamFullMeteringSweepFrames;
    initParams.min_payload_frames = kGcamMinPayloadFrames;
    initParams.max_payload_frames = kGcamMaxPayloadFrames;
    initParams.max_zsl_frames = kGcamMaxZslFrames;
    initParams.payload_frame_copy_mode = kGcamPayloadFrameCopyMode;
    initParams.image_release_callback = mGcamInputImageReleaseCallback.get();
    initParams.custom_file_saver = mGcamFileSaver.get();

    // The following callbacks are not used.
    initParams.memory_callback = nullptr;
    initParams.merge_queue_empty_callback = nullptr;
    initParams.finish_queue_empty_callback = nullptr;
    initParams.background_ae_results_callback = nullptr;

    char *use_ipu = std::getenv("USE_IPU");
    if (use_ipu != nullptr && strcmp(use_ipu, "true") == 0) {
        initParams.use_ipu = true;
    } else {
        initParams.use_ipu = false;
    }

    // There is only 1 static metadata for current device.
    std::vector<gcam::StaticMetadata> gcamMetadataList = {*mGcamStaticMetadata};

    gcam::DebugParams debugParams;
    debugParams.save_bitmask = 0;
    if (mStaticMetadata->debugParams & DEBUG_PARAM_SAVE_GCAME_INPUT_METERING) {
        debugParams.save_bitmask |= gcam::GCAM_SAVE_INPUT_METERING;
    }
    if (mStaticMetadata->debugParams & DEBUG_PARAM_SAVE_GCAME_INPUT_PAYLOAD) {
        debugParams.save_bitmask |= gcam::GCAM_SAVE_INPUT_PAYLOAD;
    }
    if (mStaticMetadata->debugParams & DEBUG_PARAM_SAVE_GCAME_TEXT) {
        debugParams.save_bitmask |= gcam::GCAM_SAVE_TEXT;
    }
    if (mStaticMetadata->debugParams & DEBUG_PARAM_SAVE_GCAME_IPU_WATERMARK) {
        debugParams.save_bitmask |= gcam::GCAM_SAVE_IPU_WATERMARK;
    }

    // Create a gcam instance.
    mGcam = std::unique_ptr<gcam::Gcam>(gcam::Gcam::Create(
            initParams,
            gcamMetadataList.data(),
            gcamMetadataList.size(),
            debugParams));
    if (mGcam == nullptr) {
        ALOGE("%s: Failed to create a Gcam instance.", __FUNCTION__);
        mGcamInputImageReleaseCallback = nullptr;
        mGcamFinalImageCallback = nullptr;
        mGcamBaseFrameCallback = nullptr;
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

    mStaticMetadata = metadata;
    return 0;
}


// Callback invoked when Gcam selects a base frame.
HdrPlusProcessingBlock::GcamBaseFrameCallback::GcamBaseFrameCallback(
        std::weak_ptr<PipelineBlock> block) : mBlock(block) {
}

void HdrPlusProcessingBlock::GcamBaseFrameCallback::Run(const gcam::IShot* shot,
        int base_frame_index, int64_t base_frame_timestamp_ns) {
    if (shot == nullptr) {
        ALOGE("%s: shot is nullptr.", __FUNCTION__);
        return;
    }

    int shotId = shot->shot_id();
    auto block = std::static_pointer_cast<HdrPlusProcessingBlock>(mBlock.lock());
    if (block != nullptr) {
        block->onGcamBaseFrameCallback(shotId, base_frame_index,
                                       base_frame_timestamp_ns);
    } else {
        ALOGE("%s: Gcam selected a base frame index %d for shot %d but block is destroyed.",
                __FUNCTION__, base_frame_index, shotId);
    }
}

HdrPlusProcessingBlock::GcamPostviewCallback::GcamPostviewCallback(
        std::weak_ptr<PipelineBlock> block) : mBlock(block) {
}

void HdrPlusProcessingBlock::GcamPostviewCallback::Run(const gcam::IShot* shot,
         gcam::YuvImage* yuv_result,
         gcam::InterleavedImageU8* rgb_result,
         gcam::GcamPixelFormat pixel_format) {
    ALOGV("%s: Gcam sent a postview for request %d", __FUNCTION__, shot->shot_id());

    auto yuvImage = std::unique_ptr<gcam::YuvImage>(yuv_result);
    auto rgbImage = std::unique_ptr<gcam::InterleavedImageU8>(rgb_result);

    auto block = std::static_pointer_cast<HdrPlusProcessingBlock>(mBlock.lock());
    if (block != nullptr) {
        block->onGcamPostview(shot->shot_id(), std::move(yuvImage), std::move(rgbImage),
                pixel_format);
    } else {
        ALOGE("%s: Gcam sent a postview for request %d but block is destroyed.",
                __FUNCTION__, shot->shot_id());
    }
}

HdrPlusProcessingBlock::GcamFileSaver::GcamFileSaver(std::weak_ptr<PipelineBlock> block) :
        mBlock(block) {
}

bool HdrPlusProcessingBlock::GcamFileSaver::operator()(const void* data, size_t byte_count,
        const std::string& filename) {
    auto block = std::static_pointer_cast<HdrPlusProcessingBlock>(mBlock.lock());
    if (block != nullptr) {
        return block->onGcamFileSaver(data, byte_count, filename);
    }

    ALOGE("%s: Gcam requests to save a file (%s) but block is destroyed.", __FUNCTION__,
            filename.c_str());
    return false;
}

HdrPlusProcessingBlock::GcamInputImageReleaseCallback::GcamInputImageReleaseCallback(
        std::weak_ptr<PipelineBlock> block) : mBlock(block) {
}

void HdrPlusProcessingBlock::GcamInputImageReleaseCallback::Run(const int64_t image_id) {
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

void HdrPlusProcessingBlock::GcamFinalImageCallback::YuvReady(
        const gcam::IShot* shot,
        gcam::YuvImage* yuv_result,
        const gcam::ExifMetadata& metadata,
        gcam::GcamPixelFormat pixel_format) {
    ALOGV("%s: Gcam sent a final image for request %d", __FUNCTION__, shot->shot_id());

    auto yuvImage = std::unique_ptr<gcam::YuvImage>(yuv_result);
    auto block = std::static_pointer_cast<HdrPlusProcessingBlock>(mBlock.lock());
    if (block != nullptr) {
        block->onGcamFinalImage(shot->shot_id(), std::move(yuvImage), pixel_format, metadata);
    } else {
        ALOGE("%s: Gcam sent a final image for request %d but block is destroyed.",
                __FUNCTION__, shot->shot_id());
    }
}

void HdrPlusProcessingBlock::addInputReference(int64_t id, Input input) {
    std::unique_lock<std::mutex> lock(mInputIdMapLock);
    auto refIt = mInputIdMap.find(id);
    if (refIt == mInputIdMap.end()) {
        mInputIdMap.emplace(id, InputAndRefCount(input));
    } else {
        refIt->second.refCount++;
    }
}

void HdrPlusProcessingBlock::removeInputReference(int64_t id) {
    std::unique_lock<std::mutex> lock(mInputIdMapLock);
    auto refIt = mInputIdMap.find(id);
    if (refIt == mInputIdMap.end()) {
      ALOGE("%s: Image %" PRId64 " never added to map.", __FUNCTION__, id);
      return;
    }

    auto ref = refIt->second;
    ref.refCount--;
    // Return input buffer back to the input queue if it is no longer used.
    // We also erase the entry from the map to keep our map bounded.
    if (!ref.refCount) {
        insertIntoInputQueue(ref.input);
        mInputIdMap.erase(refIt);
    } else if (ref.refCount < 0) {
        ALOGE("%s: Image %" PRId64 " already released.", __FUNCTION__, id);
    }
}

void HdrPlusProcessingBlock::insertIntoInputQueue(Input input) {
    {
        // This function assumes mInputQueue is already sorted, and that the
        // oldest timestamps are at the front of the queue.
        std::unique_lock<std::mutex> lock(mQueueLock);
        auto insert_it = mInputQueue.begin();
        for (; insert_it != mInputQueue.end(); insert_it++) {
            auto input_ts = input.metadata.frameMetadata->easelTimestamp;
            auto insert_it_ts = insert_it->metadata.frameMetadata->easelTimestamp;
            if (input_ts > insert_it_ts) {
              break;
            }
        }
        mInputQueue.insert(insert_it, input);
    }
    notifyWorkerThreadEvent();
}

HdrPlusProcessingBlock::ImxBuffer::ImxBuffer() : mBuffer(nullptr), mData(nullptr), mWidth(0),
        mHeight(0), mStride(0), mFormat(0) {
}

HdrPlusProcessingBlock::ImxBuffer::~ImxBuffer() {
    ImxError err;

    if (mData != nullptr) {
        err = ImxUnlockDeviceBuffer(mBuffer);
        if (err != 0){
            ALOGE("%s: Unlocking buffer failed: %d", __FUNCTION__, err);
        }
        mData = nullptr;
    }
    if (mBuffer != nullptr) {
        err = ImxDeleteDeviceBuffer(mBuffer);
        if (err != 0){
            ALOGE("%s: Deleting buffer failed: %d", __FUNCTION__, err);
        }
        mBuffer = nullptr;
    }
}

status_t HdrPlusProcessingBlock::ImxBuffer::allocate(
        ImxMemoryAllocatorHandle imxMemoryAllocatorHandle, uint32_t width, uint32_t height,
        int32_t format) {
    if (mBuffer != nullptr) {
        ALOGE("%s: buffer was already allocated.", __FUNCTION__);
        return -EEXIST;
    }

    if (format != HAL_PIXEL_FORMAT_RGB_888) {
        ALOGE("%s: format %d is not supported.", __FUNCTION__, __LINE__);
        return -EINVAL;
    }

    uint32_t bytesPerPixel = 3;
    uint32_t alignment = kImxDefaultDeviceBufferAlignment;
    uint32_t stride = ((width * bytesPerPixel + alignment - 1) / alignment) * alignment;
    uint32_t bytes = stride * height;
    ImxError err = ImxCreateDeviceBufferManaged(imxMemoryAllocatorHandle, bytes, alignment,
            kImxDefaultDeviceBufferHeap, /*flags*/0, &mBuffer);
    if (err != 0) {
        ALOGE("%s: Allocate %u bytes failed: %d", __FUNCTION__, bytes, err);
        return -ENOMEM;
    }

    mWidth = width;
    mHeight = height;
    mFormat = format;
    mStride = stride;

    return 0;
}

uint8_t* HdrPlusProcessingBlock::ImxBuffer::getData() {
    if (mData != nullptr) return mData;

    ImxError err = ImxLockDeviceBuffer(mBuffer, (void**)(&mData));
    if (err != 0) {
        ALOGE("%s: Locking buffer failed: %d", __FUNCTION__, err);
        mData = nullptr;
    }

    return mData;
}

uint32_t HdrPlusProcessingBlock::ImxBuffer::getWidth() const {
    return mWidth;
}

uint32_t HdrPlusProcessingBlock::ImxBuffer::getHeight() const {
    return mHeight;
}

uint32_t HdrPlusProcessingBlock::ImxBuffer::getStride() const {
    return mStride;
}

int32_t HdrPlusProcessingBlock::ImxBuffer::getFormat() const {
    return mFormat;
}

} // pbcamera
