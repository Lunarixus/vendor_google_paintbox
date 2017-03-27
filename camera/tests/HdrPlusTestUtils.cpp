/*
 * Copyright 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
//#define LOG_NDEBUG 0
#define LOG_TAG "HdrPlusTestUtils"
#include <utils/Log.h>

#include <chrono>
#include <inttypes.h>

#include "HdrPlusTestUtils.h"

namespace android {

namespace hdrp_test_utils {

#define RETURN_ON_METADATA_UPDATE_ERROR(_metadata, _tag, _var) \
    do { \
        status_t res = (_metadata)->update((_tag), &(_var), 1); \
        if (res != 0) { \
            ALOGE("%s: Updating tag 0x%x failed: %s (%d)", __FUNCTION__, (_tag), \
                    strerror(-res), res); \
            return res; \
        } \
    } while(0)

#define RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(_metadata, _tag, _vector) \
    do { \
        status_t res = (_metadata)->update((_tag), (_vector).data(), (_vector).size()); \
        if (res != 0) { \
            ALOGE("%s: Updating tag 0x%x failed: %s (%d)", __FUNCTION__, (_tag), \
                    strerror(-res), res); \
            return res; \
        } \
    } while(0)


status_t fillMockStaticMetadata(CameraMetadata *metadata) {
    if (metadata == nullptr) return BAD_VALUE;

    // The following are from Android Camera Metadata
    uint8_t flashInfoAvailable = ANDROID_FLASH_INFO_AVAILABLE_TRUE;
    RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_FLASH_INFO_AVAILABLE, flashInfoAvailable);

    std::vector<int32_t> sensitivityRange = {50, 12800};
    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, ANDROID_SENSOR_INFO_SENSITIVITY_RANGE,
            sensitivityRange);

    int32_t maxAnalogSensitivity = 1280;
    RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_SENSOR_MAX_ANALOG_SENSITIVITY,
            maxAnalogSensitivity);

    std::vector<int32_t> pixelArraySize = {3280, 2464};
    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE,
            pixelArraySize);

    std::vector<int32_t> activeArraySize = {0, 0, 3280, 2464};
    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE,
        activeArraySize);

    // no opticalBlackRegions

    std::vector<int32_t> availableStreamConfigurations = {
            36, 4048, 3044, ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
            32, 4048, 3044, ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
            35, 4048, 3036, ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT };
    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
            availableStreamConfigurations);

    uint8_t referenceIlluminant1 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_D65;
    RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_SENSOR_REFERENCE_ILLUMINANT1,
            referenceIlluminant1);

    uint8_t referenceIlluminant2 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_STANDARD_A;
    RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_SENSOR_REFERENCE_ILLUMINANT2,
            referenceIlluminant2);

    std::vector<camera_metadata_rational_t> calibrationTransform1 =
            { {129, 128}, {0, 128}, {0, 128},
              {0, 128}, {128, 128}, {0, 128},
              {0, 128}, {0, 128}, {128, 128} };
    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, ANDROID_SENSOR_CALIBRATION_TRANSFORM1,
            calibrationTransform1);

    std::vector<camera_metadata_rational_t> calibrationTransform2 =
            { {128, 128}, {0, 128}, {0, 128},
              {0, 128}, {128, 128}, {0, 128},
              {0, 128}, {0, 128}, {129, 128} };
    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, ANDROID_SENSOR_CALIBRATION_TRANSFORM2,
            calibrationTransform2);

    std::vector<camera_metadata_rational_t> colorTransform1 =
            { {93, 128}, {-25, 128}, {-11, 128},
              {-72, 128}, {173, 128}, {21, 128},
              {-30, 128}, {41, 128}, {71, 128} };
    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, ANDROID_SENSOR_COLOR_TRANSFORM1,
            colorTransform1);

    std::vector<camera_metadata_rational_t> colorTransform2 =
            { {138, 128}, {-40, 128}, {-36, 128},
              {-72, 128}, {212, 128}, {-15, 128},
              {-8, 128}, {26, 128}, {79, 128} };
    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, ANDROID_SENSOR_COLOR_TRANSFORM2,
            colorTransform2);

    int32_t whiteLevel = 1023;
    RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_SENSOR_INFO_WHITE_LEVEL, whiteLevel);

    uint8_t colorFilterArrangement = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGGB;
    RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT,
            colorFilterArrangement);

    float availableApertures = 2.0f;
    RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_LENS_INFO_AVAILABLE_APERTURES,
            availableApertures);

    float availableFocalLengths = 3.38000011f;
    RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS,
            availableFocalLengths);

    return OK;
}

status_t fillMockFrameMetadata(CameraMetadata *metadata, int64_t timestampNs) {
    if (metadata == nullptr) return BAD_VALUE;

    // Assign tags that HDR+ service needs with mock values.
    int64_t exposureTime = 15000000;
    RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_SENSOR_EXPOSURE_TIME, exposureTime);

    int32_t sensitivity = 100;
    RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_SENSOR_SENSITIVITY, sensitivity);

    int32_t postRawSensitivityBoost = 1;
    RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_CONTROL_POST_RAW_SENSITIVITY_BOOST,
            postRawSensitivityBoost);

    uint8_t flashMode = ANDROID_FLASH_MODE_OFF;
    RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_FLASH_MODE, flashMode);

    std::vector<float> ccGains = {1, 1, 1, 1};
    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, ANDROID_COLOR_CORRECTION_GAINS, ccGains);

    std::vector<camera_metadata_rational_t> ccTransform = { {1, 1}, {0, 1}, {0, 1}, {0, 1}, {1, 1},
                                                            {0, 1}, {0, 1}, {0, 1}, {1, 1} };
    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, ANDROID_COLOR_CORRECTION_TRANSFORM,
            ccTransform);

    std::vector<camera_metadata_rational_t> neutralColorPoint = { {0, 1}, {0, 1}, {0, 1} };
    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, ANDROID_SENSOR_NEUTRAL_COLOR_POINT,
            neutralColorPoint);

    RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_SENSOR_TIMESTAMP, timestampNs);

    uint8_t blackLevelLock = ANDROID_BLACK_LEVEL_LOCK_OFF;
    RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_BLACK_LEVEL_LOCK, blackLevelLock);

    uint8_t faceDetectMode = ANDROID_STATISTICS_FACE_DETECT_MODE_FULL;
    RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_STATISTICS_FACE_DETECT_MODE,
            faceDetectMode);

    int32_t faceId = 0;
    RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_STATISTICS_FACE_IDS, faceId);

    std::vector<int32_t> faceLandmarks = {0, 0, 0, 100, 50, 100};
    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, ANDROID_STATISTICS_FACE_LANDMARKS,
            faceLandmarks);

    std::vector<int32_t> faceRect = {0, 0, 100, 100};
    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, ANDROID_STATISTICS_FACE_RECTANGLES, faceRect);

    uint8_t faceScores = 50;
    RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_STATISTICS_FACE_SCORES, faceScores);

    uint8_t sceneFlicker = ANDROID_STATISTICS_SCENE_FLICKER_60HZ;
    RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_STATISTICS_SCENE_FLICKER, sceneFlicker);

    std::vector<double> noiseProfile = {0, 0.1, 0, 0.1, 0, 0.1, 0, 0.1};
    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, ANDROID_SENSOR_NOISE_PROFILE, noiseProfile);

    std::vector<float> dynamicBlackLevel = {100.0f, 100.0f, 100.0f, 100.0f};
    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, ANDROID_SENSOR_DYNAMIC_BLACK_LEVEL,
            dynamicBlackLevel);

    return OK;
}

int64_t getCurrentTimeNs() {
    // Get the time since epoch.
    std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();

    // Convert it to nanoseconds.
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

bool fileExist (const std::string& path) {
    std::ifstream f(path.c_str());
    return f.good();
}

status_t runCommand(const std::string& command) {
    auto pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return UNKNOWN_ERROR;
    }
    return pclose(pipe);
}

} // hdrp_test_utils

} // namespace android
