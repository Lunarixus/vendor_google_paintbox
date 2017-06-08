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
#ifndef PAINTBOX_AP_EASEL_METADATA_MANAGER_H
#define PAINTBOX_AP_EASEL_METADATA_MANAGER_H

#include <deque>
#include <map>
#include <utils/Errors.h>
#include <utils/Mutex.h>

#include "camera/CameraMetadata.h"
#include "HdrPlusTypes.h"

namespace android {

/**
 * ApEaselMetadataManager
 *
 * ApEaselMetadataManager class manages and matches CameraMetadata from AP and timestamps from
 * Easel. It assumes AP timestamp and Easel timestamp do not drift and have a very small offset.
 */
class ApEaselMetadataManager {
public:
    /*
     * maxNumFrameHistory is the number of frames' CameraMetadata and Easel timestamps to keep.
     *                    CameraMetadata and Easel timestamps that are older than maxNumFrameHistory
     *                    frames can be discard.
     */
    ApEaselMetadataManager(size_t maxNumFrameHistory);
    virtual ~ApEaselMetadataManager();

    /*
     * Convert the static metadata from CameraMetadata to pbcamera::StaticMetadata and return it.
     *
     * staticMetadataDest is an output parameter that will point to the converted
     *                    pbcamera::StaticMetadata.
     * staticMetadataSrc is the static metadata to convert from.
     */
    static status_t convertAndReturnStaticMetadata(
            std::shared_ptr<pbcamera::StaticMetadata> *staticMetadataDest,
            const std::shared_ptr<CameraMetadata> &staticMetadataSrc);
    /*
     * Add a new Easel Timestamp and if a matching cameraMetadata is found, output a
     * pbcamera::FrameMetadata.
     *
     * easelTimestampNs is the new Easel timestamp to add.
     * frameMetadata is an output parameter and will point to the matching pbcamera::FrameMetadata
     *               for the Easel timestamp, or nullptr if a matching CameraMetadta is not found.
     */
    void addEaselTimestamp(int64_t easelTimestampNs,
            std::shared_ptr<pbcamera::FrameMetadata> *frameMetadata);

    /*
     * Add a new CameraMetadata and if a matching Easel timestamp is found, output a
     * pbcamera::FrameMetadata.
     *
     * cameraMetadata is the new CameraMetadata to add.
     * frameMetadata is an output parameter and will point to the matching pbcamera::FrameMetadata
     *               for the Easel timestamp, or nullptr if a matching CameraMetadta is not found.
     */
    void addCameraMetadata(std::shared_ptr<CameraMetadata> cameraMetadata,
            std::shared_ptr<pbcamera::FrameMetadata> *frameMetadata);

    /*
     * Get camera metadata given an AP timestamp.
     *
     * cameraMetadata is an output parameter and is the camera metadata that has apTimestampNs.
     * apTimestampNs is AP timestamp of the camera metadata to get.
     *
     * Returns:
     *  OK:                 on success.
     *  NAME_NOT_FOUND:     if no camera metadata can be found with the AP timestamp. This can
     *                      happen if the camera metadata was never added via addCameraMetadata or
     *                      the camera metadata was already discarded.
     */
    status_t getCameraMetadata(std::shared_ptr<CameraMetadata> *cameraMetadata,
            int64_t apTimestampNs);

    /*
     * Set AP timestamp offset.
     *
     * AP timestamps in camera metadata may have an offset due to gyro calibration. When comparing
     * timestamps between AP and Easel, this offset should be subtracted from the sensor timestamp
     * in camera metadata.
     *
     * apTimestampOffset is the offset added to the sensor timestamp in camera metadata.
     */
    void setApTimestampOffset(int64_t apTimestampOffset);

    // Clear all managed CameraMetadata and Easel timestamps.
    void clear();

private:
    // Tolerance used to match an AP timestamp and an Easel timestamp.
    const int64_t kApEaselTimestampDiffToleranceNs = 2000000; // 2 ms

    // Defines a matching pair of pbcamera::FrameMetadata and CameraMetadata that belong to the
    // same frame.
    struct ApEaselMetadata {
        // Frame metadata to pass to HDR+ service.
        std::shared_ptr<pbcamera::FrameMetadata> pbFrameMetadata;
        // Result metadata of a frame captured in AP.
        std::shared_ptr<CameraMetadata> cameraMetadata;
    };

    // Convert a CameraMetadata to pbcamera::FrameMetadata.
    status_t convertAndReturnPbFrameMetadata(
            std::shared_ptr<pbcamera::FrameMetadata> *frameMetadata,
            const std::shared_ptr<CameraMetadata>& camerMetadata);

    // Try to add a pair of CameraMetadata and easelTimestamp and if they match, return a converted
    // pbcamera::FrameMetadata.
    bool tryAddingApEaselMetadataLocked(std::shared_ptr<CameraMetadata> cameraMetadata,
            int64_t easelTimestamp, std::shared_ptr<pbcamera::FrameMetadata> *frameMetadata);

    // Protecting the follow containers.
    Mutex mLock;

    // Number of frame's metadata to keep.
    size_t mMaxNumFrameHistory;

    // Easel timestamps that do not have a matching CameraMetadata yet.
    std::deque<int64_t> mPendingEaselTimestamps;

    // CameraMetadata that do not have a matching Easel timestamp yet.
    std::deque<std::shared_ptr<CameraMetadata>> mPendingCameraMetadata;

    // Map from AP timestamps to its matching ApEaselMetadata.
    std::map<int64_t, ApEaselMetadata> mApTimestampToMetadataMap;

    // AP timestamp offset added to the sensor timestamp. This needs to be subtracted from AP
    // timestamp when comparing AP and Easel timestamps.
    int64_t mApTimestampOffsetNs;
};

} // namespace android

#endif // PAINTBOX_AP_EASEL_METADATA_MANAGER_H
