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

#ifndef PAINTBOX_HDR_PLUS_CLIENT_H
#define PAINTBOX_HDR_PLUS_CLIENT_H

#include <deque>
#include <utils/Errors.h>
#include <utils/Mutex.h>
#include <utils/Thread.h>
#include <stdint.h>
#include <queue>

#include "ApEaselMetadataManager.h"
#include "easelcontrol.h"
#include "hardware/camera3.h"
#include "HdrPlusClientListener.h"
#include "HdrPlusProfiler.h"
#include "MessengerToHdrPlusService.h"
#include "MessengerListenerFromHdrPlusService.h"

namespace android {

class NotifyFrameMetadataThread;

/**
 * HdrPlusClient
 *
 * HdrPlusClient class can be used to connect to HDR+ service to perform HDR+ processing on
 * Paintbox.
 */
class HdrPlusClient : public pbcamera::MessengerListenerFromHdrPlusService {
public:
    HdrPlusClient();
    virtual ~HdrPlusClient();

    /*
     * Return if Easel is present on the device.
     *
     * If Easel is not present, all other calls to HdrPlusClient are invalid.
     */
    bool isEaselPresentOnDevice() const;

    /*
     * Power on Easel.
     *
     * Must be called before other following methods.
     */
    status_t powerOnEasel();

    /*
     * Suspend Easel.
     *
     * Put Easel on suspend mode.
     */
    status_t suspendEasel();

    /*
     * Resume Easel.
     *
     * Resume Easel from suspend mode.
     */
    status_t resumeEasel();

    /*
     * Set Easel bypass MIPI rate
     *
     * Can be called when Easel is powered on or resumed, for Easel to start bypassing sensor data
     * to AP.
     */
    status_t setEaselBypassMipiRate(uint32_t cameraId, uint32_t outputPixelClkHz);

    /*
     * Connect to HDR+ service.
     *
     * listener is the listener to receive callbacks from HDR+ client.
     *
     * Returns:
     *  0:          on success.
     *  -EEXIST:    if it's already connected.
     *  -ENODEV:    if connecting failed due to a serious error.
     */
    status_t connect(HdrPlusClientListener *listener);

    // Disconnect from HDR+ service.
    void disconnect();

    /*
     * Set the static metadata of current camera device.
     *
     * Must be called after connect() and before configuring streams.
     *
     * staticMetadata is the static metadata of current camera device.
     *
     * Returns:
     *  0:         on success.
     *  -ENODEV:   if HDR+ service is not connected.
     */
    status_t setStaticMetadata(const camera_metadata_t &staticMetadata);

    /*
     * Configure streams.
     *
     * Must be called when configuration changes including input (sensor) resolution and format, and
     * output resolutions and formats.
     *
     * inputConfig contains the information about the input frames or sensor configurations.
     * outputConfigs is a vector of output stream configurations.
     *
     * Returns:
     *  0:          on success.
     *  -EINVAL:    if outputConfigs is empty or the configurations are not supported.
     *  -ENODEV:    if HDR+ service is not connected.
     */
    status_t configureStreams(const pbcamera::InputConfiguration &inputConfig,
            const std::vector<pbcamera::StreamConfiguration> &outputConfigs);

    /*
     * Enable or disable ZSL HDR+ mode.
     *
     * When ZSL HDR+ mode is enabled, Easel will capture ZSL RAW buffers. ZSL HDR+ mode should be
     * disabled to reduce power consumption when HDR+ processing is not necessary, e.g in video
     * mode.
     *
     * enabled is a flag indicating whether to enable ZSL HDR+ mode.
     *
     * Returns:
     *  0:          on success.
     *  -ENODEV:    if HDR+ service is not connected, or streams are not configured.
     */
    status_t setZslHdrPlusMode(bool enabled);

    /*
     * Submit a capture request for HDR+ outputs.
     *
     * For each output buffer in CaptureRequest, it will be returned in a CaptureResult via
     * HdrPlusClientListener::onCaptureResult(). HdrPlusClientListener::onCaptureResult() may be
     * invoked multiple times to return all output buffers in one CaptureRequest. Each output
     * buffer will be returned in CaptureResult only once.
     *
     * request is a CaptureRequest containing output buffers to be filled by HDR+ service.
     *
     * Returns:
     *  0:              on success.
     *  -EINVAL:        if the request is invalid such as containing invalid stream IDs.
     */
    status_t submitCaptureRequest(pbcamera::CaptureRequest *request);

    /*
     * Send an input buffer to HDR+ service. This is used when HDR+ service's input buffers come
     * from the client rather than MIPI.
     *
     * inputBuffer is the input buffer to send to HDR+ service. After this method returns, the
     *             buffer has been copied (with DMA) to HDR+ service and the caller has the
     *             ownership of the buffer.
     */
    void notifyInputBuffer(const pbcamera::StreamBuffer &inputBuffer, int64_t timestampNs);

    /*
     * Notify about result metadata of a frame that AP captured. This may be called multiple times
     * for a frame to send multiple partial metadata and lastMetadata must be false except for the
     * last partial metadata. When there is only one metadata for a frame, lastMetadata must be
     * true.
     *
     * frameNumber is a unique frame number that the metadata belong to.
     * resultMetadata is the result metadata of a frame that AP captured.
     * lastMetadata is a flag indicating whether this is the last metadata for the frame number.
     */
    void notifyFrameMetadata(uint32_t frameNumber, const camera_metadata_t &resultMetadata,
            bool lastMetadata=true);

private:
    static const size_t kMaxNumFrameHistory = 32;

    // TODO: Figure out where this comes from but this ratio converts from QC's op_pixel_clk to
    // the rate that Easel expects.
    static constexpr float kApEaselMipiRateConversion = 0.0000025f;

#if !USE_LIB_EASEL
    // Used to connect Easel control. Only needed for TCP/IP mock.
    const char *mDefaultServerHost = "localhost";
#endif

    // Callbacks from HDR+ service start here.
    // Override pbcamera::MessengerListenerFromHdrPlusService
    void notifyFrameEaselTimestamp(int64_t easelTimestampNs) override;
    void notifyDmaCaptureResult(pbcamera::DmaCaptureResult *result) override;
    // Callbacks from HDR+ service end here.

    // Activate Easel.
    status_t activateEasel();

    // Deactivate Easel.
    void deactivateEasel();

    // Easel controls
    bool mIsEaselPresent;
    EaselControlClient mEaselControl;
    bool mEaselControlOpened;
    bool mEaselActivated;
    Mutex mEaselControlLock; // Protecting Easel control variables above.

    // EaselMessenger to send messages to HDR+ service.
    pbcamera::MessengerToHdrPlusService mMessengerToService;

    // Callbacks to invoke from HdrPlusClient.
    Mutex mClientListenerLock;
    HdrPlusClientListener *mClientListener;

    // Outstanding requests that the client has not received the corresponding results to.
    struct PendingRequest {
        pbcamera::CaptureRequest request;
        DECLARE_PROFILER_TIMER(timer, "HDR+ request");
    };

    Mutex mPendingRequestsLock;
    std::deque<PendingRequest> mPendingRequests;

    ApEaselMetadataManager mApEaselMetadataManager = {kMaxNumFrameHistory};

    // Map from frame number to partial metadata received so far.
    Mutex mFrameNumPartialMetadataMapLock;
    std::map<uint32_t, std::shared_ptr<CameraMetadata>> mFrameNumPartialMetadataMap;

    // Static black level.
    std::array<float, 4> mBlackLevelPattern; // android.sensor.blackLevelPattern

    sp<NotifyFrameMetadataThread> mNotifyFrameMetadataThread;
};

/**
 * NotifyFrameMetadataThread
 *
 * A thread to send frame metadata to Easel to avoid deadlocks caused by sending messages back
 * to Easel on Easel callback thread.
 */
class NotifyFrameMetadataThread : public Thread {
public:
    // messenger must be valid during this object's lifetime.
    NotifyFrameMetadataThread(pbcamera::MessengerToHdrPlusService *messenger);
    virtual ~NotifyFrameMetadataThread();

    /*
     * Queue a frame metadata that will be sent to Easel asynchronously.
     */
    void queueFrameMetadata(std::shared_ptr<pbcamera::FrameMetadata> frameMetadata);

    // Override Thread::requestExit to request thread exit.
    void requestExit() override;

private:

    // Threadloop to wait on new frame metadata and send frame metadata to Easel.
    virtual bool threadLoop() override;

    // MessengerToHdrPlusService for sending messages to Easel.
    pbcamera::MessengerToHdrPlusService *mMessenger;

    // Mutext to protect variables as noted.
    std::mutex mEventLock;

    // Condition variable for new frame metadata or thread exit request. Must be protected by
    // mEventLock.
    std::condition_variable mEventCond;

    // Frame metadata queue pending to be sent to Easel. Must be protected by mEventLock.
    std::queue<std::shared_ptr<pbcamera::FrameMetadata>> mFrameMetadataQueue;

    // Whether exit has been requested. Must be protected by mEventLock.
    bool mExitRequested;
};

} // namespace android

#endif // PAINTBOX_HDR_PLUS_CLIENT_H