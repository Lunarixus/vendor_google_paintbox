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

#ifndef PAINTBOX_HDR_PLUS_CLIENT_LISTENER_H
#define PAINTBOX_HDR_PLUS_CLIENT_LISTENER_H

namespace android {

/*
 * HdrPlusClientListener defines callbacks that will be invoked by HdrPlusClient for events like
 * returning capture results.
 */
class HdrPlusClientListener {
public:
    virtual ~HdrPlusClientListener() {};

    /*
     * Invoked when a CaptureResult, containing a subset or all output buffers for a CaptureRequest,
     * is received. This may be invoked multiple times for one CaptureRequest but each CaptureResult
     * will contain distinct output buffers that have not been received yet.
     */
    virtual void onCaptureResult(pbcamera::CaptureResult *result,
            const camera_metadata_t &resultMetadata) = 0;

    /*
     * Invoked when a failed CaptureResult, containing a subset or all output buffers for a
     * CaptureRequest, is received. Output buffers in a failed capture result may contain garbage
     * data. This may be invoked multiple times for one CaptureRequest but each CaptureResult
     * will contain distinct output buffers that have not been received yet.
     */
    virtual void onFailedCaptureResult(pbcamera::CaptureResult *failedResult) = 0;
};

} // namespace android

#endif // PAINTBOX_HDR_PLUS_CLIENT_LISTENER_H