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

#ifndef PAINTBOX_HDR_PLUS_CLIENT_TEST_UTILS_H
#define PAINTBOX_HDR_PLUS_CLIENT_TEST_UTILS_H

#include <fstream>
#include <memory>

#include <CameraMetadata.h>

using ::android::hardware::camera::common::V1_0::helper::CameraMetadata;

namespace android {

namespace hdrp_test_utils {
    /*
     * Fill static metadata with mocking values.
     *
     * Returns:
     *  OK:         on success.
     *  BAD_VALUE:  if metadata is null.
     */
    status_t fillMockStaticMetadata(CameraMetadata *metadata);

    /*
     * Fill frame metadata with mocking values except the specified timestamp.
     *
     * timestampNs is the sensor timestamp to set in the created CameraMetadata.
     *
     * Returns:
     *  OK:         on success.
     *  BAD_VALUE:  if metadata is null.
     */
    status_t fillMockFrameMetadata(CameraMetadata *metadata, int64_t timestampNs);

    /*
     * Return elapsed time since epoch in nanoseconds.
     */
    int64_t getCurrentTimeNs();

    /*
     * Returns true if file exists.
     */
    bool fileExist (const std::string& path);

    /*
     * Runs a command and returns the exit value;
     */
    status_t runCommand(const std::string& command);

} // hdrp_test_utils

} // namespace android

#endif // PAINTBOX_HDR_PLUS_CLIENT_TEST_UTILS_H
