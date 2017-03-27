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

#ifndef PAINTBOX_HDR_PLUS_CLIENT_UTILS_H
#define PAINTBOX_HDR_PLUS_CLIENT_UTILS_H

#include <utils/Errors.h>

#include "HdrPlusTypes.h"

namespace android {
namespace hdrplus_client_utils {

/*
 * Write the image buffer to a .ppm file.
 *
 * filename is the filename of the .ppm file and should include ".ppm" in the end.
 * streamConfig is the stream configuration of the buffer.
 * buffer is the buffer to be saved to a .ppm file.
 *
 * Returns
 *  OK:             if the file is saved successfully.
 *  BAD_VALUE:      if the format is not support or the stream configuration is invalid.
 *  NO_INIT:        if it failed to open the file.
 */
status_t writePpm(const std::string &filename, const pbcamera::StreamConfiguration &streamConfig,
        const pbcamera::StreamBuffer &buffer);

} // hdrplus_client_utils
} // namespace android

#endif // PAINTBOX_HDR_PLUS_CLIENT_UTILS_H
