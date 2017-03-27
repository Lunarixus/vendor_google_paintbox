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
#define LOG_TAG "ApEaselMetadataManager"
#include <utils/Log.h>

#include <cutils/properties.h>

#include <inttypes.h>
#include <stdlib.h>

#include "ApEaselMetadataManager.h"
#include "system/camera_metadata.h"

namespace android {

ApEaselMetadataManager::ApEaselMetadataManager(size_t maxNumFrameHistory) :
        mMaxNumFrameHistory(maxNumFrameHistory) {
}

ApEaselMetadataManager::~ApEaselMetadataManager() {
}

/*
 * Return a pointer of type T to the data in a camera_metadata_entry.
 */
template<typename T>
static T *getMetadataEntryData(const camera_metadata_entry& entry) {
    void *data = nullptr;
    switch (entry.type) {
        case TYPE_BYTE:
            data = entry.data.u8;
            break;
        case TYPE_INT32:
            data = entry.data.i32;
            break;
        case TYPE_FLOAT:
            data = entry.data.f;
            break;
        case TYPE_INT64:
            data = entry.data.i64;
            break;
        case TYPE_DOUBLE:
            data = entry.data.d;
            break;
        default:
            ALOGE("%s: Unknown entry type: %d.", __FUNCTION__, entry.type);
            data = nullptr;
    }

    return static_cast<T*>(data);
}

/*
 * Find the value of a tag in metadata and fill a specified variable with the value. Rational
 * numbers will be converted to type T.
 *
 * dest is the pointer to data to fill.
 * metadataSrc is the metadata to get the value of a tag from.
 * tag is the metadata tag to get the value of.
 *
 * Returns:
 *  0:          on success.
 *  BAD_VALUE:  dest is null, metadataSrc is null, or an entry with the tag cannot be found.
 */
template<typename T>
static status_t fillMetadataValue(T *dest,
        const std::shared_ptr<CameraMetadata> &metadataSrc, camera_metadata_tag tag) {
    if (dest == nullptr) {
        ALOGE("%s: dest is null.", __FUNCTION__);
        return BAD_VALUE;
    }

    if (metadataSrc == nullptr) {
        ALOGE("%s: metadataSrc is null.", __FUNCTION__);
        return BAD_VALUE;
    }

    // Find the entry and check the count.
    camera_metadata_entry entry = metadataSrc->find(tag);
    if (entry.count != 1) {
        ALOGE("%s: %s has %d values (expecting 1).", __FUNCTION__,
                get_camera_metadata_section_name(tag), static_cast<int>(entry.count));
        return BAD_VALUE;
    }

    if (entry.type == TYPE_RATIONAL) {
        // If the entry is a rational number, convert it to the dest data type.
        (*dest) = static_cast<T>(entry.data.r->numerator) / entry.data.r->denominator;
    } else {
        *dest = *getMetadataEntryData<T>(entry);
    }

    return OK;
}

/*
 * Find the values of a tag in metadata and fill a specified array with the values. Rational
 * numbers will be converted to type T.
 *
 * dest is the pointer to array to fill.
 * metadataSrc is the metadata to get the value of a tag from.
 * tag is the metadata tag to get the value of.
 *
 * Returns:
 *  0:          on success.
 *  BAD_VALUE:  dest is null, metadataSrc is null, or an entry with the tag cannot be found.
 */
template<typename T, size_t SIZE>
static status_t fillMetadataArray(std::array<T, SIZE> *dest,
        const std::shared_ptr<CameraMetadata> &metadataSrc, camera_metadata_tag tag) {
    if (dest == nullptr) {
        ALOGE("%s: dest is null.", __FUNCTION__);
        return BAD_VALUE;
    }

    if (metadataSrc == nullptr) {
        ALOGE("%s: metadataSrc is null.", __FUNCTION__);
        return BAD_VALUE;
    }

    // Find the entry and check the count.
    camera_metadata_entry entry = metadataSrc->find(tag);
    if (entry.count != dest->size()) {
        ALOGE("%s: %s has %d values (expecting %d).", __FUNCTION__,
                get_camera_metadata_section_name(tag), static_cast<int>(entry.count),
                static_cast<int>(dest->size()));
        return BAD_VALUE;
    }

    for (size_t i = 0; i < entry.count; i++) {
        if (entry.type == TYPE_RATIONAL) {
            // If the entry is a rational number, convert it to the dest data type.
            (*dest)[i] = static_cast<T>(entry.data.r[i].numerator) / entry.data.r[i].denominator;
        } else {
            (*dest)[i] = getMetadataEntryData<T>(entry)[i];
        }
    }

    return OK;
}

/*
 * Find the values of a tag in metadata and fill a specified vector with the values. Rational
 * numbers will be converted to type T.
 *
 * dest is the pointer to vector to fill.
 * metadataSrc is the metadata to get the value of a tag from.
 * tag is the metadata tag to get the value of.
 *
 * Returns:
 *  0:          on success.
 *  BAD_VALUE:  dest is null, metadataSrc is null, or an entry with the tag cannot be found.
 */
template<typename T>
static status_t fillMetadataVector(std::vector<T> *dest,
        const std::shared_ptr<CameraMetadata> &metadataSrc, camera_metadata_tag tag) {
    if (dest == nullptr) {
        ALOGE("%s: dest is null.", __FUNCTION__);
        return BAD_VALUE;
    }

    if (metadataSrc == nullptr) {
        ALOGE("%s: metadataSrc is null.", __FUNCTION__);
        return BAD_VALUE;
    }

    camera_metadata_entry entry = metadataSrc->find(tag);
    for (size_t i = 0; i < entry.count; i++) {
        if (entry.type == TYPE_RATIONAL) {
            // If the entry is a rational number, convert it to the dest data type.
            dest->push_back(static_cast<T>(entry.data.r[i].numerator) /
                                           entry.data.r[i].denominator);
        } else {
            dest->push_back(getMetadataEntryData<T>(entry)[i]);
        }
    }

    return OK;
}

/*
 * Find the values of a tag in metadata and fill a specified vector of arrays with the values.
 * Rational numbers will be converted to type T.
 *
 * dest is the pointer to vector of arrays to fill.
 * metadataSrc is the metadata to get the value of a tag from.
 * tag is the metadata tag to get the value of.
 *
 * Returns:
 *  0:          on success.
 *  BAD_VALUE:  dest is null, metadataSrc is null, or an entry with the tag cannot be found.
 */
template<typename T, size_t SIZE>
static status_t fillMetadataVectorArray(std::vector<std::array<T, SIZE>> *dest,
        const std::shared_ptr<CameraMetadata> &metadataSrc, camera_metadata_tag tag) {
    if (dest == nullptr) {
        ALOGE("%s: dest is null.", __FUNCTION__);
        return BAD_VALUE;
    }

    if (metadataSrc == nullptr) {
        ALOGE("%s: metadataSrc is null.", __FUNCTION__);
        return BAD_VALUE;
    }

    // Find the entry and check the count.
    camera_metadata_entry entry = metadataSrc->find(tag);
    if (entry.count % SIZE != 0) {
        ALOGE("%s: %s has %d values (should be multiples of %d).", __FUNCTION__,
                get_camera_metadata_section_name(tag), static_cast<int>(entry.count),
                static_cast<int>(SIZE));
        return BAD_VALUE;
    }

    for (size_t i = 0; i < entry.count / SIZE; i++) {
        std::array<T, SIZE> array;
        for (size_t j = 0; j < SIZE; j++) {
            if (entry.type == TYPE_RATIONAL) {
                // If the entry is a rational number, convert it to the dest data type.
                array[j] = static_cast<T>(entry.data.r[j].numerator) / entry.data.r[j].denominator;
            } else {
                array[j] = getMetadataEntryData<T>(entry)[i * SIZE + j];
            }
        }
       dest->push_back(array);
    }

    return OK;
}

/*
 * Find the values of a tag in metadata and fill a specified array of arrays with the values.
 * Rational numbers will be converted to type T.
 *
 * dest is the pointer to array of arrays to fill.
 * metadataSrc is the metadata to get the value of a tag from.
 * tag is the metadata tag to get the value of.
 *
 * Returns:
 *  0:          on success.
 *  BAD_VALUE:  dest is null, metadataSrc is null, or an entry with the tag cannot be found.
 */
template<typename T, size_t SIZE1, size_t SIZE2>
static status_t fillMetadataArrayArray(std::array<std::array<T, SIZE2>, SIZE1> *dest,
        const std::shared_ptr<CameraMetadata> &metadataSrc, camera_metadata_tag tag) {
    if (dest == nullptr) {
        ALOGE("%s: dest is null.", __FUNCTION__);
        return BAD_VALUE;
    }

    if (metadataSrc == nullptr) {
        ALOGE("%s: metadataSrc is null.", __FUNCTION__);
        return BAD_VALUE;
    }

    // Find the data and check the count.
    camera_metadata_entry entry = metadataSrc->find(tag);
    if (entry.count != SIZE1 * SIZE2) {
        ALOGE("%s: %s has %d values but expecting %d.", __FUNCTION__,
                get_camera_metadata_section_name(tag), static_cast<int>(entry.count),
                static_cast<int>(SIZE1 * SIZE2));
        return BAD_VALUE;
    }

    for (size_t i = 0; i < entry.count; i++) {
        if (entry.type == TYPE_RATIONAL) {
            // If the entry is a rational number, convert it to the dest data type.
            (*dest)[i / SIZE2][i % SIZE2] = static_cast<T>(entry.data.r[i].numerator) /
                                                           entry.data.r[i].denominator;
        } else {
            (*dest)[i / SIZE2][i % SIZE2] = getMetadataEntryData<T>(entry)[i];
        }
    }

    return OK;
}

#define RETURN_ERROR_ON_ERROR(_expr) \
    do { \
        status_t res = (_expr); \
        if (res < 0) { \
            ALOGE("%s: %d: Failed: %s (%d)", __FUNCTION__, __LINE__, strerror(-res), res); \
            return res; \
        } \
    } while(0)


status_t ApEaselMetadataManager::convertAndReturnStaticMetadata(
        std::shared_ptr<pbcamera::StaticMetadata> *staticMetadataDest,
        const std::shared_ptr<CameraMetadata> &staticMetadataSrc) {
    if (staticMetadataDest == nullptr) {
        ALOGE("%s: staticMetadataDest cannot be null.", __FUNCTION__);
        return BAD_VALUE;
    }

    if (staticMetadataSrc == nullptr) {
        ALOGE("%s: staticMetadataSrc cannot be null.", __FUNCTION__);
        return BAD_VALUE;
    }

    std::shared_ptr<pbcamera::StaticMetadata> dest = std::make_shared<pbcamera::StaticMetadata>();

    RETURN_ERROR_ON_ERROR(fillMetadataValue(&dest->flashInfoAvailable, staticMetadataSrc,
            ANDROID_FLASH_INFO_AVAILABLE));
    RETURN_ERROR_ON_ERROR(fillMetadataArray(&dest->sensitivityRange, staticMetadataSrc,
            ANDROID_SENSOR_INFO_SENSITIVITY_RANGE));
    RETURN_ERROR_ON_ERROR(fillMetadataValue(&dest->maxAnalogSensitivity, staticMetadataSrc,
            ANDROID_SENSOR_MAX_ANALOG_SENSITIVITY));
    RETURN_ERROR_ON_ERROR(fillMetadataArray(&dest->pixelArraySize, staticMetadataSrc,
            ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE));
    RETURN_ERROR_ON_ERROR(fillMetadataArray(&dest->activeArraySize, staticMetadataSrc,
            ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE));
    RETURN_ERROR_ON_ERROR(fillMetadataVectorArray(&dest->opticalBlackRegions, staticMetadataSrc,
            ANDROID_SENSOR_OPTICAL_BLACK_REGIONS));
    RETURN_ERROR_ON_ERROR(fillMetadataVectorArray(&dest->availableStreamConfigurations,
            staticMetadataSrc, ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS));
    RETURN_ERROR_ON_ERROR(fillMetadataValue(&dest->referenceIlluminant1, staticMetadataSrc,
            ANDROID_SENSOR_REFERENCE_ILLUMINANT1));
    RETURN_ERROR_ON_ERROR(fillMetadataValue(&dest->referenceIlluminant2, staticMetadataSrc,
            ANDROID_SENSOR_REFERENCE_ILLUMINANT2));
    RETURN_ERROR_ON_ERROR(fillMetadataArray(&dest->calibrationTransform1, staticMetadataSrc,
            ANDROID_SENSOR_CALIBRATION_TRANSFORM1));
    RETURN_ERROR_ON_ERROR(fillMetadataArray(&dest->calibrationTransform2, staticMetadataSrc,
            ANDROID_SENSOR_CALIBRATION_TRANSFORM2));
    RETURN_ERROR_ON_ERROR(fillMetadataArray(&dest->colorTransform1, staticMetadataSrc,
            ANDROID_SENSOR_COLOR_TRANSFORM1));
    RETURN_ERROR_ON_ERROR(fillMetadataArray(&dest->colorTransform2, staticMetadataSrc,
            ANDROID_SENSOR_COLOR_TRANSFORM2));
    RETURN_ERROR_ON_ERROR(fillMetadataValue(&dest->whiteLevel, staticMetadataSrc,
            ANDROID_SENSOR_INFO_WHITE_LEVEL));
    RETURN_ERROR_ON_ERROR(fillMetadataValue(&dest->colorFilterArrangement, staticMetadataSrc,
            ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT));
    RETURN_ERROR_ON_ERROR(fillMetadataVector(&dest->availableApertures, staticMetadataSrc,
            ANDROID_LENS_INFO_AVAILABLE_APERTURES));
    RETURN_ERROR_ON_ERROR(fillMetadataVector(&dest->availableFocalLengths, staticMetadataSrc,
            ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS));
    RETURN_ERROR_ON_ERROR(fillMetadataArray(&dest->shadingMapSize, staticMetadataSrc,
            ANDROID_LENS_INFO_SHADING_MAP_SIZE));
    RETURN_ERROR_ON_ERROR(fillMetadataValue(&dest->focusDistanceCalibration, staticMetadataSrc,
            ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION));

    *staticMetadataDest = dest;
    return OK;
}

status_t ApEaselMetadataManager::convertAndReturnPbFrameMetadata(
        std::shared_ptr<pbcamera::FrameMetadata> *frameMetadata,
        const std::shared_ptr<CameraMetadata> &cameraMetadata) {
    if (frameMetadata == nullptr) {
        ALOGE("%s: cameraMetadata cannot be null.", __FUNCTION__);
        return BAD_VALUE;
    }

    if (cameraMetadata == nullptr) {
        ALOGE("%s: cameraMetadata cannot be null.", __FUNCTION__);
        return BAD_VALUE;
    }

    std::shared_ptr<pbcamera::FrameMetadata> dest = std::make_shared<pbcamera::FrameMetadata>();

    RETURN_ERROR_ON_ERROR(fillMetadataValue(&dest->exposureTime, cameraMetadata,
            ANDROID_SENSOR_EXPOSURE_TIME));
    RETURN_ERROR_ON_ERROR(fillMetadataValue(&dest->sensitivity, cameraMetadata,
            ANDROID_SENSOR_SENSITIVITY));
    RETURN_ERROR_ON_ERROR(fillMetadataValue(&dest->postRawSensitivityBoost,
            cameraMetadata, ANDROID_CONTROL_POST_RAW_SENSITIVITY_BOOST));
    RETURN_ERROR_ON_ERROR(fillMetadataValue(&dest->flashMode, cameraMetadata,
            ANDROID_FLASH_MODE));
    RETURN_ERROR_ON_ERROR(fillMetadataArray(&dest->colorCorrectionGains, cameraMetadata,
            ANDROID_COLOR_CORRECTION_GAINS));
    RETURN_ERROR_ON_ERROR(fillMetadataArray(&dest->colorCorrectionTransform,
            cameraMetadata, ANDROID_COLOR_CORRECTION_TRANSFORM));
    RETURN_ERROR_ON_ERROR(fillMetadataArray(&dest->neutralColorPoint, cameraMetadata,
            ANDROID_SENSOR_NEUTRAL_COLOR_POINT));
    RETURN_ERROR_ON_ERROR(fillMetadataValue(&dest->timestamp, cameraMetadata,
            ANDROID_SENSOR_TIMESTAMP));
    RETURN_ERROR_ON_ERROR(fillMetadataValue(&dest->blackLevelLock, cameraMetadata,
            ANDROID_BLACK_LEVEL_LOCK));
    RETURN_ERROR_ON_ERROR(fillMetadataValue(&dest->faceDetectMode, cameraMetadata,
            ANDROID_STATISTICS_FACE_DETECT_MODE));
    RETURN_ERROR_ON_ERROR(fillMetadataVector(&dest->faceIds, cameraMetadata,
            ANDROID_STATISTICS_FACE_IDS));
    RETURN_ERROR_ON_ERROR(fillMetadataVectorArray(&dest->faceLandmarks, cameraMetadata,
            ANDROID_STATISTICS_FACE_LANDMARKS));
    RETURN_ERROR_ON_ERROR(fillMetadataVectorArray(&dest->faceRectangles, cameraMetadata,
            ANDROID_STATISTICS_FACE_RECTANGLES));
    RETURN_ERROR_ON_ERROR(fillMetadataVector(&dest->faceScores, cameraMetadata,
            ANDROID_STATISTICS_FACE_SCORES));
    RETURN_ERROR_ON_ERROR(fillMetadataValue(&dest->sceneFlicker, cameraMetadata,
            ANDROID_STATISTICS_SCENE_FLICKER));
    RETURN_ERROR_ON_ERROR(fillMetadataArrayArray(&dest->noiseProfile, cameraMetadata,
            ANDROID_SENSOR_NOISE_PROFILE));
    RETURN_ERROR_ON_ERROR(fillMetadataArray(&dest->dynamicBlackLevel, cameraMetadata,
            ANDROID_SENSOR_DYNAMIC_BLACK_LEVEL));
    RETURN_ERROR_ON_ERROR(fillMetadataVector(&dest->lensShadingMap, cameraMetadata,
            ANDROID_STATISTICS_LENS_SHADING_MAP));
    RETURN_ERROR_ON_ERROR(fillMetadataValue(&dest->focusDistance, cameraMetadata,
            ANDROID_LENS_FOCUS_DISTANCE));

    *frameMetadata = dest;
    return OK;
}

bool ApEaselMetadataManager::tryAddingApEaselMetadataLocked(
        std::shared_ptr<CameraMetadata> cameraMetadata,
        int64_t easelTimestamp, std::shared_ptr<pbcamera::FrameMetadata> *frameMetadata) {
    ALOGV("%s", __FUNCTION__);

    // Get the AP timestamp in camera metadata.
    camera_metadata_entry entry = cameraMetadata->find(ANDROID_SENSOR_TIMESTAMP);
    if (entry.count == 0) {
        ALOGE("%s: Cannot find ANDROID_SENSOR_TIMESTAMP.", __FUNCTION__);
        return false;
    }
    int64_t apTimestamp = entry.data.i64[0];

    entry = cameraMetadata->find(ANDROID_SENSOR_EXPOSURE_TIME);
    if (entry.count == 0) {
        ALOGE("%s: Cannot find ANDROID_SENSOR_EXPOSURE_TIME.", __FUNCTION__);
        return false;
    }
    int64_t exposureTime = entry.data.i64[0];

    // Easel start exposure time is the Easel vsync time - frame exposure time.
    int64_t easelStartExpTime = easelTimestamp - exposureTime;

    ALOGV("%s: easelStartExpTime %" PRId64 " apTimestamp %" PRId64 " exposureTime %" PRId64,
            __FUNCTION__, easelStartExpTime, apTimestamp, exposureTime);

    // TODO: Enforce timestamp matching when timestamp is accurate. b/35399985
    bool noTimestampMatching = property_get_bool("persist.camera.hdrplus.notimestampmatching",
            false);

    // Check if they are within the tolerence.
    if (noTimestampMatching ||
            llabs(apTimestamp - easelStartExpTime) <= kApEaselTimestampDiffToleranceNs) {
        // They match so create an entry in mApTimestampToMetadataMap.
        std::shared_ptr<pbcamera::FrameMetadata> pbFrameMetadata;
        status_t res = convertAndReturnPbFrameMetadata(&pbFrameMetadata, cameraMetadata);
        if (res != OK) {
            return false;
        }

        // Add the matching pair to mApTimestampToMetadataMap.
        ApEaselMetadata apEaselMetadata;
        apEaselMetadata.pbFrameMetadata = pbFrameMetadata;
        apEaselMetadata.pbFrameMetadata->easelTimestamp = easelTimestamp;
        apEaselMetadata.cameraMetadata = cameraMetadata;
        mApTimestampToMetadataMap.emplace(pbFrameMetadata->timestamp, apEaselMetadata);

        // Remove oldest entry if it exceeds the max number.
        if (mApTimestampToMetadataMap.size() > mMaxNumFrameHistory) {
            mApTimestampToMetadataMap.erase(mApTimestampToMetadataMap.begin());
        }

        if (frameMetadata != nullptr) {
            *frameMetadata = pbFrameMetadata;
        }

        return true;
    }

    // Not a match.
    return false;
}

void ApEaselMetadataManager::addEaselTimestamp(int64_t easelTimestampNs,
        std::shared_ptr<pbcamera::FrameMetadata> *frameMetadata) {
    Mutex::Autolock l(mLock);

    // Look for a matching CameraMetadata.
    auto cameraMetadata = mPendingCameraMetadata.begin();
    while (cameraMetadata != mPendingCameraMetadata.end()) {
        if (tryAddingApEaselMetadataLocked(*cameraMetadata, easelTimestampNs, frameMetadata)) {
            // Match found.
            mPendingCameraMetadata.erase(cameraMetadata);
            return;
        }
        cameraMetadata++;
    }

    // No matching CameraMetadata found. Put the Easel timestamp in the pending queue to match
    // up later.
    mPendingEaselTimestamps.push_back(easelTimestampNs);
    if (mPendingEaselTimestamps.size() > mMaxNumFrameHistory) {
        mPendingEaselTimestamps.pop_front();
    }
}

void ApEaselMetadataManager::addCameraMetadata(std::shared_ptr<CameraMetadata> cameraMetadata,
        std::shared_ptr<pbcamera::FrameMetadata> *frameMetadata) {
    Mutex::Autolock l(mLock);

    // Look for a matching Easel timestamp.
    auto easelTimestamp = mPendingEaselTimestamps.begin();
    while (easelTimestamp != mPendingEaselTimestamps.end()) {
        if (tryAddingApEaselMetadataLocked(cameraMetadata, *easelTimestamp, frameMetadata)) {
            // Match found.
            mPendingEaselTimestamps.erase(easelTimestamp);
            return;
        }
        easelTimestamp++;
    }

    // No matching Easel timestamp found. Put the CameraMetadata in the pending queue to match up
    // later.
    mPendingCameraMetadata.push_back(cameraMetadata);
    if (mPendingCameraMetadata.size() > mMaxNumFrameHistory) {
        mPendingCameraMetadata.pop_front();
    }
}

status_t ApEaselMetadataManager::getCameraMetadata(std::shared_ptr<CameraMetadata> *cameraMetadata,
        int64_t apTimestampNs) {
    if (cameraMetadata == nullptr) return BAD_VALUE;

    Mutex::Autolock l(mLock);

    auto metadataIter = mApTimestampToMetadataMap.find(apTimestampNs);
    if (metadataIter == mApTimestampToMetadataMap.end()) {
        *cameraMetadata = nullptr;
        return NAME_NOT_FOUND;
    }

    *cameraMetadata = metadataIter->second.cameraMetadata;
    return OK;
}

void ApEaselMetadataManager::clear() {
    Mutex::Autolock l(mLock);

    mPendingEaselTimestamps.clear();
    mPendingCameraMetadata.clear();
    mApTimestampToMetadataMap.clear();
}

} // namespace android
