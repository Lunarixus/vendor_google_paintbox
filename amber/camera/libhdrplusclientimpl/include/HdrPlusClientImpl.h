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

#ifndef PAINTBOX_HDR_PLUS_CLIENT_IMPL_H
#define PAINTBOX_HDR_PLUS_CLIENT_IMPL_H

#include <deque>
#include <unordered_map>
#include <utils/Errors.h>
#include <utils/Mutex.h>
#include <utils/Thread.h>
#include <stdint.h>
#include <queue>

#include "ApEaselMetadataManager.h"
#include "hardware/camera3.h"
#include "HdrPlusClient.h"
#include "HdrPlusProfiler.h"
#include "MessengerToHdrPlusService.h"
#include "MessengerListenerFromHdrPlusService.h"

namespace android {

class NotifyFrameMetadataThread;
class TimerCallbackThread;

/**
 * HdrPlusClientImpl
 *
 * HdrPlusClientImpl class can be used to connect to HDR+ service to perform HDR+ processing on
 * Paintbox.
 */
class HdrPlusClientImpl : public HdrPlusClient, public pbcamera::MessengerListenerFromHdrPlusService {
public:
    // listener is the listener to receive callbacks from HDR+ client. listener must be valid
    // during the life cycle of HdrPlusClient.
    HdrPlusClientImpl(HdrPlusClientListener *listener);
    virtual ~HdrPlusClientImpl();

    /*
     * The recommended way to create an HdrPlusClientImpl instance is via
     * EaselManagerClient::openHdrPlusClientAsync() or EaselManagerClient::openHdrPlusClientAsync().
     * EaselManagerClient will make sure Easel is in a valid state to open an HDR+ client. To close
     * an HdrPlusClientImpl, use EaselManagerClient::closeHdrPlusClient.
     */

    /*
     * Connect to HDR+ service.
     *
     * If EaselManagerClient is used to create the HdrPlusClientImpl, it is already connected.
     *
     * Returns:
     *  0:          on success.
     *  -EEXIST:    if it's already connected.
     *  -ENODEV:    if connecting failed due to a serious error.
     */
    status_t connect();

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
     * requestMetadata is the metadata for this request.
     *
     * Returns:
     *  0:              on success.
     *  -EINVAL:        if the request is invalid such as containing invalid stream IDs.
     */
    status_t submitCaptureRequest(pbcamera::CaptureRequest *request,
            const CameraMetadata &requestMetadata);

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

    /*
     * Notify atrace / systrace about an event.
     *
     * trace the trace name.
     * cookie the id of the event.
     * begin whether it is the beginning of the event (1) or end (0)
     */
    void notifyAtrace(const std::string &trace, int32_t cookie, int32_t begin) override;

    /*
     * Notify Easel has encountered a fatal error and HDR+ client should stop sending messages
     * to Easel.
     */
    void nofityEaselFatalError() override;

private:
    static const size_t kMaxNumFrameHistory = 64;

    // Timeout duration for an HDR+ request to come back from Easel.
    static const int64_t kDefaultRequestTimerMs = 2000; // 2 seconds

    // Callbacks from HDR+ service start here.
    // Override pbcamera::MessengerListenerFromHdrPlusService
    void notifyFrameEaselTimestamp(int64_t easelTimestampNs) override;
    void notifyDmaCaptureResult(pbcamera::DmaCaptureResult *result) override;
    void notifyServiceClosed() override;
    void notifyShutter(uint32_t requestId, int64_t apSensorTimestampNs) override;
    void notifyDmaMakernote(pbcamera::DmaMakernote *dmaMakernote) override;
    void notifyDmaPostview(uint32_t requestId, void *dmaHandle, uint32_t width,
            uint32_t height, uint32_t stride, int32_t format) override;
    void notifyDmaFileDump(const std::string &filename, DmaBufferHandle dmaHandle,
            uint32_t dmaDataSize) override;
    void notifyNextCaptureReady(uint32_t requestId) override;
    // Callbacks from HDR+ service end here.

    // Disconnect from HDR+ service.
    void disconnect();

    // Return and mark all pending requests as failed. Must called with mClientListenerLock held.
    void failAllPendingRequestsLocked();

    // Handle the situation when an HDR+ request has not completed within a timeout duration.
    void handleRequestTimeout(uint32_t id);

    // Update camera result metadata based on HDR+ result.
    status_t updateResultMetadata(std::shared_ptr<CameraMetadata> *cameraMetadata,
            const std::string &makernote);

    // Create a directory for file dump if it doesn't exist, and return the final path.
    status_t createFileDumpDirectory(const std::string &baseDir,
        const std::vector<std::string>& paths, std::string *finalPath);

    // Create a directory if it doesn't exist.
    status_t createDir(const std::string &dir);

    // Split filename, separated by "/", into a vector of strings.
    std::vector<std::string> splitPath(const std::string &filename);

    // Write data to a file.
    void writeData(const std::string& path, std::vector<char> &data);

    // Return if the frame metadata is valid.
    bool isValidFrameMetadata(const std::shared_ptr<CameraMetadata> &frameMetadata);

    // EaselMessenger to send messages to HDR+ service.
    pbcamera::MessengerToHdrPlusService mMessengerToService;

    // Callbacks to invoke from HdrPlusClientImpl.
    HdrPlusClientListener *mClientListener;

    enum OutputBufferStatus {
        // Output buffer request is sent to Easel.
        OUTPUT_BUFFER_REQUESTED = 0,
        // Output buffer is captured and transferred from Easel.
        OUTPUT_BUFFER_CAPTURED,
        // Output buffer failed.
        OUTPUT_BUFFER_FAILED,
    };

    // Outstanding requests that the client has not received the corresponding results to.
    struct PendingRequest {
        pbcamera::CaptureRequest request;
        // stream ID -> output buffer status.
        std::unordered_map<uint32_t, OutputBufferStatus> outputBufferStatuses;
        std::string makernote;
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

    // A thread to invoke a callback function after a specified duration has been reached.
    sp<TimerCallbackThread> mTimerCallbackThread;

    // If HDR+ service is closed unexpectedly. Once mServiceClosed is true, it can no longer send
    // messages to HDR+ service.
    std::atomic<bool> mServiceFatalErrorState;

    // If disconnecting from HDR+ service has started.
    std::atomic<bool> mDisconnecting;

    // Static metadata of current camera.
    std::unique_ptr<pbcamera::StaticMetadata> mStaticMetadata;

    // Whether or not to ignore timeouts.
    bool mIgnoreTimeouts;
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

/**
 * TimerCallbackThread
 *
 * A thread to invoke a callback function after a specified duration has been reached.
 */
class TimerCallbackThread : public Thread {
public:
    TimerCallbackThread(std::function<void(uint32_t)> callback);
    virtual ~TimerCallbackThread();

    /*
     * Add a new timer.
     *
     * id is the ID of the timer. callback function will be invoked with id. id must be unique.
     * durationMs is the duration of the timer in milliseconds.
     *
     * Returns:
     *   OK on success.
     *   ALREADY_EXISTS if id already exists in pending timers.
     */
    status_t addTimer(uint32_t id, uint64_t durationMs);

    // Cancel a timer.
    void cancelTimer(uint32_t id);

    // Override Thread::requestExit to request thread exit.
    void requestExit() override;

private:
    // Wait 5 seconds if there is no timer.
    static const int64_t kEmptyTimerWaitTimeMs = 5000;

    // Threadloop to wait on new timer or exit request.
    virtual bool threadLoop() override;

    // Return the current time (CLOCK_BOOTTIME) in milliseconds.
    int64_t getCurrentTimeMs();

    // Return the wait time for the timer that has the earliest expiration time. Must be protected
    // by mTimerLock.
    int64_t getWaitTimeMsLocked();

    // Callback to invoke once a timer has expired.
    std::function<void(uint32_t)> mCallback;

    // Mutext to protect variables as noted.
    std::mutex mTimerLock;

    // Condition variable for new timer or thread exit request. Must be protected by mTimerLock.
    std::condition_variable mTimerCond;

    // Map from timer id to expiration time in milliseconds. Must be protected by mTimerLock.
    std::unordered_map<uint32_t, int64_t> mTimers;

    // Whether exit has been requested. Must be protected by mTimerLock.
    bool mExitRequested;
};

} // namespace android

#endif // PAINTBOX_HDR_PLUS_CLIENT_IMPL_H
