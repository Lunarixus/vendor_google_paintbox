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
#define LOG_TAG "HdrPlusClientTest"
#include <log/log.h>

#include <cutils/properties.h>
#include <gtest/gtest.h>
#include <inttypes.h>
#include <sstream>
#include <utils/Condition.h>
#include <vector>

#include "EaselManagerClient.h"
#include "HdrPlusClient.h"
#include "HdrPlusClientListener.h"
#include "HdrPlusClientUtils.h"
#include "HdrPlusTestBurstInput.h"
#include "HdrPlusTestUtils.h"

namespace android {

const char kBurstInputDir[] =
        "/data/nativetest/hdrplus_client_tests/bursts/0080_20170616_120819_772/";

const char kCompiledGraphDir[] =
        "/data/paintbox/compiled_graph/";

const char kOutputDumpDir[] =
        "/data/nativetest/hdrplus_client_tests/dump/";

const char kSkipConfigureServer[] = "persist.hdrplus_client_test.skip_configure_server";

const char kDoNotPoweronEasel[] = "camera.hdrplus.donotpoweroneasel";

const int32_t kInvalidFd = -1;

const char kDefaultOutputDiffThreshold[] = "0.01";

const char kDefaultNumRequests[] = "1";

class HdrPlusClientTest : public HdrPlusClientListener,
                          public EaselManagerClientListener,
                          public ::testing::Test {

public:
    // Not used because HDR+ client will be created synchronously.
    void onOpened(std::unique_ptr<HdrPlusClient>) override {};
    void onOpenFailed(status_t) override {};
    void onFatalError() override {
        ASSERT_FALSE(true) << "HDR+ client has a fatal error.";
    };

    // Easel manager client callbacks
    void onEaselFatalError(std::string errMsg) {
        ASSERT_FALSE(true) << "Easel has encountered a fatal error: " << errMsg;
    };

    // Override HdrPlusClientListener::onCaptureResult to receive capture results.
    void onCaptureResult(pbcamera::CaptureResult *result, const camera_metadata_t &resultMetadata)
            override {
        if (result == nullptr) return;

        ALOGV("%s: Got a capture result for request %d.", __FUNCTION__, result->requestId);

        dumpOutput(result);
        pullCompiledGraph();
        verifyOutput(result);

        Mutex::Autolock l(mCaptureResultLock);

        // Push to capture result queue and signal main thread about a new capture result.
        mCaptureResults.push_back(*result);
        mCaptureResultCond.signal();
    }

    // Override HdrPlusClientListener::onCaptureResult to receive failed capture results.
    void onFailedCaptureResult(pbcamera::CaptureResult *failedResult) override {
        if (failedResult == nullptr) return;

        ALOGE("%s: Got a failed capture result for request %d.", __FUNCTION__,
                failedResult->requestId);

        // Return the buffer back to stream.
        for (auto buffer : failedResult->outputBuffers) {
            EXPECT_TRUE(false) << "Recieved a failed capture result for stream " << buffer.streamId;
            returnStreamBuffer(&buffer);
        }
    }

    void onShutter(uint32_t requestId, int64_t apSensorTimestampNs) {
        ALOGI("%s: Got a shutter callback for request %u timestamp %" PRId64, __FUNCTION__,
                requestId, apSensorTimestampNs);
    }

    void onNextCaptureReady(uint32_t requestId) {
        ALOGI("%s: Got a next capture ready callback for request %u", __FUNCTION__, requestId);
    }

    void onPostview(uint32_t requestId, std::unique_ptr<std::vector<uint8_t>> postview,
            uint32_t width, uint32_t height, uint32_t stride, int32_t format) {
        ALOGI("%s: Got a postview %ux%u format %d for request %u.", __FUNCTION__, width, height,
                format, requestId);
    }

protected:
    // Constants for input configuration.
    static const uint32_t kDefaultInputWidth = 4048;
    static const uint32_t kDefaultInputHeight = 3036;
    static const uint32_t kDefaultInputFormat = HAL_PIXEL_FORMAT_RAW10;
    static const uint32_t kDefaultNumInputBuffer = 1;

    // Easel HDR+ will not pick a base frame of the same timestamp twice. Adding a timestamp offset
    // for each request's input buffers to workaround the restriction.
    static const int64_t kTimestampOffsetPerRequestNs = 1e9; // 1 seconds.

    // Constants for output configurations.
    const uint32_t kDefaultOutputFormats[2] = { HAL_PIXEL_FORMAT_YCrCb_420_SP,
                                                HAL_PIXEL_FORMAT_RAW16 };
    static const uint32_t kDefaultNumOutputBuffer = 3;

    // Number of capture requests to submit.
    static const uint32_t kNumTestCaptureRequests = 3; // Must be <= kDefaultNumOutputBuffer.

    // Time to wait for capture results.
    static const uint32_t kResultTimeoutMs = 300000; // 300 seconds

    // Define a stream used in the test.
    struct HdrPlusClientTestStream {
        // Configuration of the stream.
        pbcamera::StreamConfiguration config;

        // A vector of buffers allocated for the stream.
        std::vector<std::vector<uint8_t>> allBuffers;

        // A vector of buffers that are available to be used.
        std::vector<void*> availableBuffers;

        // Size of each buffer in bytes.
        uint32_t bufferSizeBytes;
    };

    // Input and output streams
    std::unique_ptr<HdrPlusClientTestStream> mInputStream;
    std::vector<std::unique_ptr<HdrPlusClientTestStream>> mOutputStreams;

    std::unique_ptr<EaselManagerClient> mEaselManagerClient;
    std::unique_ptr<HdrPlusClient> mClient;

    // Flag indicating if the test is connected to HdrPlusClient.
    bool mConnected;

    // CaptureResult returned via onCaptureResult().
    Mutex mCaptureResultLock;
    Condition mCaptureResultCond;
    std::vector<pbcamera::CaptureResult> mCaptureResults;

    // Verifying against only one golden image is supported.
    bool mVerifyOutput;
    std::string mGoldenImagePath;

    // Override ::testing::Test::SetUp() to set up the test.
    virtual void SetUp() override {
        mEaselManagerClient = EaselManagerClient::create();
        configureCameraServer(true);
        destroyAllStreams();
        mConnected = false;
        mVerifyOutput = false;
        mGoldenImagePath.erase();
    }

    // Override ::testing::Test::TearDown() to free test resources.
    virtual void TearDown() override {
        disconnectClient();
        destroyAllStreams();
        configureCameraServer(false);
    }

    // Configures CameraServer into testing mode or functional mode.
    // If test_mode is true, camera server will be set to test mode
    // otherwise functional mode.
    status_t configureCameraServer(bool test_mode) {
        if (property_get_bool(kSkipConfigureServer, false))
            return OK;

        status_t ret = property_set(kDoNotPoweronEasel, test_mode ? "1" : "0");
        if (ret != OK) {
            ALOGE("Could not set %s to 1", kDoNotPoweronEasel);
            return ret;
        }

        return hdrp_test_utils::runCommand("pkill -f camera; sleep 1;");
    }

    // Connect to client.
    status_t connectClient() {
        status_t res = mEaselManagerClient->open();
        if (res != 0) {
            ALOGE("%s: Powering on Easel failed: %s (%d).", __FUNCTION__, strerror(-res), res);
            return res;
        }

        res = mEaselManagerClient->resume(this);
        if (res != 0) {
            ALOGE("%s: Resuming Easel failed: %s (%d).", __FUNCTION__, strerror(-res), res);
            disconnectClient();
            return res;
        }

        res = mEaselManagerClient->openHdrPlusClient(this, &mClient);
        if (res != 0) {
            ALOGE("%s: Opening HDR+ client failed: %s (%d).", __FUNCTION__, strerror(-res), res);
            disconnectClient();
            return res;
        }

        mConnected = true;
        return 0;
    }

    void disconnectClient() {
        mEaselManagerClient->closeHdrPlusClient(std::move(mClient));
        mEaselManagerClient->suspend();
        mConnected = false;
    }

    void pullCompiledGraph() {
        if (hdrp_test_utils::fileExist("/vendor/bin/ezlsh")) {

            // Check if dumping PCG is enabled.
            bool dumpPcg = property_get_bool("persist.hdrplus_client_test.dump_pcg", false);

            // Return if dump is not enabled.
            if (!dumpPcg) return;

            ALOGI("Pull compiled graph to %s", kCompiledGraphDir);
            std::stringstream ss;
            ss << "ezlsh pull " << kCompiledGraphDir << " " << kOutputDumpDir;
            std::string command = ss.str();
            status_t res = hdrp_test_utils::runCommand(command);
            ASSERT_EQ(OK, res) << command << " failed";
        }
    }

    void dumpOutput(pbcamera::CaptureResult *result) {
        if (result == nullptr) return;

        // Check if dumping HDR+ YUV output is enabled.
        bool dumpOutput = property_get_bool("persist.hdrplus_client_test.dump_output", false);

        // Return if dump is not enabled.
        if (!dumpOutput) return;

        // Create the output directory if it doesn't exist.
        struct stat st = {};
        if (stat(kOutputDumpDir, &st) != OK) {
            if (mkdir(kOutputDumpDir, 700) != OK) {
                ALOGE("%s: Creating the output directory (%s) failed: %s (%d)", __FUNCTION__,
                        kOutputDumpDir, strerror(errno), -errno);
                return;
            }
        }

        // Dump each buffer in the result.
        for (auto & buffer : result->outputBuffers) {
            // Find the output configuration for this buffer.
            pbcamera::StreamConfiguration *config = nullptr;
            for (auto & stream : mOutputStreams) {
                if (stream->config.id == buffer.streamId) {
                    config = &stream->config;
                    break;
                }
            }

            if (config == nullptr) {
                ALOGE("%s: Could not find the stream for this buffer (stream %d)", __FUNCTION__,
                    buffer.streamId);
                continue;
            }

            char buf[FILENAME_MAX] = {};
            snprintf(buf, sizeof(buf), "%ss_%d_%d_%dx%d.ppm", kOutputDumpDir,
                    result->requestId, buffer.streamId, config->image.width, config->image.height);
            hdrplus_client_utils::writePpm(buf, *config, buffer);
        }
    }

    void verifyOutput(pbcamera::CaptureResult *result) {
        if (result == nullptr || !mVerifyOutput) return;

        char value[256];
        property_get("persist.hdrplus_client_test.compare_threshold", value,
                kDefaultOutputDiffThreshold);
        float diffThreshold = std::atof(value);

        // Verify each buffer in the result.
        for (auto & buffer : result->outputBuffers) {
            // Find the output configuration for this buffer.
            pbcamera::StreamConfiguration *config = nullptr;
            for (auto & stream : mOutputStreams) {
                if (stream->config.id == buffer.streamId) {
                    config = &stream->config;
                    break;
                }
            }

            if (config == nullptr) {
                ALOGE("%s: Could not find the stream for this buffer (stream %d)", __FUNCTION__,
                    buffer.streamId);
                continue;
            }

            float diffRatio = 1.0;
            ASSERT_EQ(hdrplus_client_utils::comparePpm(mGoldenImagePath, *config, buffer,
                    &diffRatio), OK);
            ASSERT_LE(diffRatio, diffThreshold);

            fprintf(stderr, "diffRatio %f (threshold %f)\n", diffRatio, diffThreshold);
        }
    }

    // Create a stream.
    status_t createStream(std::unique_ptr<HdrPlusClientTestStream> *stream, int id, uint32_t width,
            uint32_t height, uint32_t format, uint32_t numBuffers) {
        if (stream == nullptr) return -EINVAL;

        std::unique_ptr<HdrPlusClientTestStream> newStream =
                std::make_unique<HdrPlusClientTestStream>();

        newStream->config.id = id;
        newStream->config.image.width = width;
        newStream->config.image.height = height;
        newStream->config.image.format = format;

        pbcamera::PlaneConfiguration plane;

        // Find out how many bytes to allocate for each buffer and plane.
        switch(format) {
            case HAL_PIXEL_FORMAT_RAW10:
                newStream->bufferSizeBytes = width * height * 10 / 8;
                plane.stride = width * 10 / 8;
                plane.scanline = height;
                newStream->config.image.planes.push_back(plane);
                break;
            case HAL_PIXEL_FORMAT_YCrCb_420_SP:
                newStream->bufferSizeBytes = width * height * 3 / 2;
                // Y plane
                plane.stride = width;
                plane.scanline = height;
                newStream->config.image.planes.push_back(plane);

                // UV plane
                plane.stride = width;
                plane.scanline = height / 2;
                newStream->config.image.planes.push_back(plane);
                break;
            case HAL_PIXEL_FORMAT_RAW16:
                newStream->bufferSizeBytes = width * height * 2;
                plane.stride = width * 2;
                plane.scanline = height;
                newStream->config.image.planes.push_back(plane);
                break;
            default:
                ALOGE("%s: Stream format %u not supported.", __FUNCTION__, format);
                return -EINVAL;
        }

        // Allocate buffers for the stream.
        newStream->allBuffers.resize(numBuffers);
        for (auto &buffer : newStream->allBuffers) {
            buffer.resize(newStream->bufferSizeBytes);
            newStream->availableBuffers.push_back(&buffer[0]);
        }

        *stream = std::move(newStream);

        return 0;
    }

    // Create all streams with default resolution and format.
    status_t createAllStreams() {
        std::vector<uint32_t> outputFormats;
        for (uint32_t i = 0; i < sizeof(kDefaultOutputFormats) / sizeof(uint32_t); i++) {
            outputFormats.push_back(kDefaultOutputFormats[i]);
        }

        return createAllStreams(kDefaultInputWidth, kDefaultInputHeight, kDefaultInputFormat,
                kDefaultInputWidth, kDefaultInputHeight, outputFormats);
    }

    // Create all streams with specified resolution and format.
    status_t createAllStreams(uint32_t inputWidth, uint32_t inputHeight, uint32_t inputFormat,
            uint32_t outputWidth, uint32_t outputHeight,
            const std::vector<uint32_t> &outputFormats) {
        int streamId = 0;

        // Create input stream
        int res = createStream(&mInputStream, streamId++, inputWidth, inputHeight, inputFormat,
                kDefaultNumInputBuffer);
        if (res != 0) {
            ALOGE("%s: Creating input stream failed: res %ux%u format %u numBuffers %d",
                    __FUNCTION__, inputWidth, inputHeight, inputFormat, kDefaultNumInputBuffer);
            destroyAllStreams();
            return res;
        }

        // Create output streams using the input resolution.
        for (auto outputFormat : outputFormats) {
            std::unique_ptr<HdrPlusClientTestStream> stream;
            res = createStream(&stream, streamId++, outputWidth, outputHeight, outputFormat,
                    kDefaultNumOutputBuffer);
            if (res != 0) {
                ALOGE("%s: Creating output stream failed: res %ux%u format %u numBuffers %d",
                        __FUNCTION__, outputWidth, outputHeight,
                        outputFormat, kDefaultNumOutputBuffer);
                destroyAllStreams();
                return res;
            }

            mOutputStreams.push_back(std::move(stream));
        }

        return 0;
    }

    // Destroy all streams.
    void destroyAllStreams() {
        returnAllBuffersInReceivedRequests();

        mInputStream = nullptr;
        mOutputStreams.clear();
    }

    // Configure stream.
    status_t configureStreams() {
        std::vector<pbcamera::StreamConfiguration> outputConfigs;
        for (auto &stream : mOutputStreams) {
            outputConfigs.push_back(stream->config);
        }

        pbcamera::InputConfiguration inputConfig;
        inputConfig.isSensorInput = false;
        inputConfig.streamConfig = mInputStream->config;

        status_t res = mClient->configureStreams(inputConfig, outputConfigs);
        if (res != OK) {
            ALOGE("%s: Configuring stream failed: %s (%d)", __FUNCTION__, strerror(-res), res);
            return res;
        }

        res = mClient->setZslHdrPlusMode(true);
        if (res != 0) {
            ALOGE("%s: Enable HDR+ mode failed: %s (%d).", __FUNCTION__, strerror(-res), res);
            return res;
        }

        return OK;
    }

    // Return whether the test has received a requested output buffer.
    bool isRequestBufferReceivedLocked(uint32_t requestId,
            const pbcamera::StreamBuffer &requestBuffer) {
        for (auto result : mCaptureResults) {
            if (result.requestId == requestId) {
                for (auto resultBuffer : result.outputBuffers) {
                    if (resultBuffer.streamId == requestBuffer.streamId) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    // Return a buffer to the stream it belongs to.
    void returnStreamBuffer(pbcamera::StreamBuffer *streamBuffer) {
        if (streamBuffer == nullptr) return;

        // Check if it belongs to the input stream.
        if (streamBuffer->streamId == mInputStream->config.id) {
            mInputStream->availableBuffers.push_back(streamBuffer->data);
            return;
        } else {
            // Check if it belongs to one of the output streams.
            for (auto & stream : mOutputStreams) {
                if (streamBuffer->streamId == stream->config.id) {
                    stream->availableBuffers.push_back(streamBuffer->data);
                    return;
                }
            }
        }
    }

    void returnAllBuffersInReceivedRequests() {
        // Erase the results for this request.
        for (auto result : mCaptureResults) {
            for (auto buffer : result.outputBuffers) {
                returnStreamBuffer(&buffer);
            }
        }
        mCaptureResults.clear();
    }

    // Wait until the test receives all results of a request or a specified amount of time has
    // elapsed between results. If it doesn't time out, results of the request will be removed
    // from the result queue.
    status_t waitForResults(pbcamera::CaptureRequest request, uint64_t timeoutMs) {
        Mutex::Autolock l(mCaptureResultLock);

        // Wait until all request's output buffers are back.
        while (request.outputBuffers.size() > 0) {
            auto requestBufferIter = request.outputBuffers.begin();
            while(requestBufferIter != request.outputBuffers.end()) {
                if (isRequestBufferReceivedLocked(request.id, *requestBufferIter) == true) {
                    // Found a requested buffer in results. Remove it from the request.
                    requestBufferIter = request.outputBuffers.erase(requestBufferIter);
                } else {
                    requestBufferIter++;
                }
            }

            // If not all output buffers are back, wait for the next result.
            if (request.outputBuffers.size() > 0) {
                int res = mCaptureResultCond.waitRelative(mCaptureResultLock,
                        /*nsecs_t*/timeoutMs * 1000000);
                if (res != 0) {
                    ALOGE("%s: Waiting for a result failed. %s (%d).", __FUNCTION__,
                            strerror(-res), res);
                    return res;
                }
            }
        }

        // Erase the results for this request.
        auto resultIter = mCaptureResults.begin();
        while (resultIter != mCaptureResults.end()) {
            if (resultIter->requestId == request.id) {
                // Return output buffers to streams.
                for (auto buffer : resultIter->outputBuffers) {
                    returnStreamBuffer(&buffer);
                }

                resultIter = mCaptureResults.erase(resultIter);
            } else {
                resultIter++;
            }
        }

        return 0;
    }

    // Test capture requests with specified output formats and number of requests.
    void testCaptureRequests(const std::vector<uint32_t> &outputFormats, uint32_t numRequests,
                             bool backToBackProcessing) {
        ASSERT_EQ(connectClient(), OK);

        HdrPlusTestBurstInput burstInput(kBurstInputDir);
        uint32_t numBurstInputs = burstInput.getNumberOfBurstInputs();
        ASSERT_NE(numBurstInputs, (uint32_t)0) << "Cannot find DNG files in " << kBurstInputDir;

        // Get static metadata.
        CameraMetadata staticMetadata;
        ASSERT_EQ(burstInput.loadStaticMetadataFromFile(&staticMetadata), OK);
        const camera_metadata_t* metadata = staticMetadata.getAndLock();
        ASSERT_EQ(mClient->setStaticMetadata(*metadata), OK);
        staticMetadata.unlock(metadata);

        // Get RAW width and height.
        camera_metadata_entry entry = staticMetadata.find(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE);
        ASSERT_EQ(entry.count, (uint32_t)2);
        int32_t rawWidth = entry.data.i32[0];
        int32_t rawHeight = entry.data.i32[1];

        std::vector<pbcamera::CaptureRequest> submittedRequests;

        // TODO: support multiple outputs and other formats.
        ASSERT_EQ(outputFormats.size(), (uint32_t)1);
        ASSERT_EQ(outputFormats[0], HAL_PIXEL_FORMAT_YCrCb_420_SP);

        // Find the largest stream configuration for YUV output
        int32_t yuvWidth = 0, yuvHeight = 0;
        entry = staticMetadata.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
        for (uint32_t i = 0; i < entry.count; i += 4) {
            if (entry.data.i32[i] == HAL_PIXEL_FORMAT_YCBCR_420_888 &&
                    entry.data.i32[i + 3] == 0) {
                if (entry.data.i32[i + 1] >= yuvWidth && entry.data.i32[i + 2] >= yuvHeight) {
                    yuvWidth = entry.data.i32[i + 1];
                    yuvHeight = entry.data.i32[i + 2];
                }
            }
        }

        // Create streams based on raw resolution.
        ASSERT_EQ(createAllStreams(rawWidth, rawHeight, kDefaultInputFormat,
                yuvWidth, yuvHeight, outputFormats), OK);

        // Configure default streams.
        ASSERT_EQ(configureStreams(), OK);

        for (uint32_t i = 0; i < numRequests; i++) {
            // Send input buffers and camera metadata.
            // The burst input frame 0 is the most recent frame. We need to load the oldest frame
            // first.
            for (int32_t j = numBurstInputs - 1; j >= 0; j--) {
                // Load buffer and metadata from files.
                CameraMetadata frameMetadata;
                ASSERT_EQ(burstInput.loadRaw10BufferAndMetadataFromFile(
                        mInputStream->availableBuffers[0], mInputStream->bufferSizeBytes,
                        &frameMetadata, j), OK);

                // Get the timestamp of the frame from metadata
                // Easel SOF timestamp = AP sensor timestamp + exposure time.
                camera_metadata_entry entry = frameMetadata.find(ANDROID_SENSOR_TIMESTAMP);
                ASSERT_EQ(entry.count, (uint32_t)1)
                        << "Cannot find timestamp in metadata for frame " << j;
                int64_t timestampNs = entry.data.i64[0];

                if (kTimestampOffsetPerRequestNs != 0) {
                    timestampNs += (kTimestampOffsetPerRequestNs * i);
                    ASSERT_EQ(frameMetadata.update(ANDROID_SENSOR_TIMESTAMP, &timestampNs, 1), 0);
                }

                entry = frameMetadata.find(ANDROID_SENSOR_EXPOSURE_TIME);
                ASSERT_EQ(entry.count, (uint32_t)1)
                        << "Cannot find exposure time in metadata for frame " << j;
                int64_t exposureTimeNs = entry.data.i64[0];

                // Send an input buffer
                pbcamera::StreamBuffer inputBuffer = {};
                inputBuffer.streamId = mInputStream->config.id;
                inputBuffer.dmaBufFd = kInvalidFd;
                inputBuffer.data = mInputStream->availableBuffers[0];
                inputBuffer.dataSize = mInputStream->bufferSizeBytes;
                mClient->notifyInputBuffer(inputBuffer, timestampNs + exposureTimeNs);

                // Create and send a CameraMetadata
                const camera_metadata_t* metadata = frameMetadata.getAndLock();
                mClient->notifyFrameMetadata(numBurstInputs - 1 - j, *metadata);
                frameMetadata.unlock(metadata);
            }

            CameraMetadata requestMetadata;

            // Disable digital zoom by setting crop region to full active array.
            entry = staticMetadata.find(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE);
            ASSERT_EQ(entry.count, static_cast<uint32_t>(4)) << "Active array size has " <<
                    entry.count << " entries. (Expecting 4)";
            int32_t cropRegion[4] = { 0, 0, entry.data.i32[2], entry.data.i32[3] };
            requestMetadata.update(ANDROID_SCALER_CROP_REGION, cropRegion, 4);
            int32_t aeExposureCompensation = 0;
            requestMetadata.update(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION,
                    &aeExposureCompensation, 1);

            // Prepare a request
            pbcamera::CaptureRequest request = {};
            request.id = i;

            // Prepare a buffer for each output stream for the capture
            for (auto &outputStream : mOutputStreams) {

                // Make sure there is output buffer available.
                ASSERT_GT(outputStream->availableBuffers.size(), (uint32_t)0) <<
                        "No output buffer available for stream " << outputStream->config.id;

                pbcamera::StreamBuffer buffer = {};
                buffer.streamId = outputStream->config.id;
                buffer.dmaBufFd = kInvalidFd;
                buffer.data = outputStream->availableBuffers.back();
                buffer.dataSize = outputStream->bufferSizeBytes;
                outputStream->availableBuffers.pop_back();

                request.outputBuffers.push_back(buffer);
            }

            // Issue a capture request.
            ASSERT_EQ(mClient->submitCaptureRequest(&request, requestMetadata), OK);

            if (backToBackProcessing) {
                fprintf(stderr, "Submit request %d/%d\n", i, numRequests);
                ASSERT_EQ(waitForResults(request, kResultTimeoutMs), OK);
                fprintf(stderr, "Request %d done!\n", i);
            }
            else {
                submittedRequests.push_back(request);
            }
        }

        if (!backToBackProcessing) {
           // Wait for results of all requests.
           for (auto request : submittedRequests) {
               ASSERT_EQ(waitForResults(request, kResultTimeoutMs), OK);
           }
        }

        // TODO: Verify other combinations of output buffers.

        disconnectClient();
    }
};

// Test HDR+ client connection.
TEST_F(HdrPlusClientTest, Connect) {
    ASSERT_EQ(connectClient(), 0);
    disconnectClient();
}

// Test stream configuration.
TEST_F(HdrPlusClientTest, StreamConfiguration) {
    ASSERT_EQ(connectClient(), 0);

    // Create streams with default configurations.
    ASSERT_EQ(createAllStreams(), 0);

    // Configuring streams before setting static metadata should fail.
    ASSERT_EQ(configureStreams(), -ENODEV);

    // Load static metadata from a file.
    CameraMetadata staticMetadata;
    HdrPlusTestBurstInput burstInput(kBurstInputDir);
    ASSERT_EQ(burstInput.loadStaticMetadataFromFile(&staticMetadata), OK);

    // Set static metadata.
    const camera_metadata_t* metadata = staticMetadata.getAndLock();
    ASSERT_EQ(mClient->setStaticMetadata(*metadata), OK);
    staticMetadata.unlock(metadata);

    // Configuring stream again after setting static metadata should succeed.
    ASSERT_EQ(configureStreams(), 0);

    // TODO: test more resolutions.

    disconnectClient();
}

// Test capture requests with NV21 and RAW16 output.
TEST_F(HdrPlusClientTest, CaptureRequest) {
    std::vector<uint32_t> outputFormats;
    for (uint32_t i = 0; i < sizeof(kDefaultOutputFormats) / sizeof(uint32_t); i++) {
        outputFormats.push_back(kDefaultOutputFormats[i]);
    }
    testCaptureRequests(outputFormats, kNumTestCaptureRequests, false);
}

// Test capture requests with NV21.
TEST_F(HdrPlusClientTest, CaptureSingleYuv) {
    std::vector<uint32_t> outputFormats = { HAL_PIXEL_FORMAT_YCrCb_420_SP };
    mVerifyOutput = true;
    mGoldenImagePath.append(kBurstInputDir);
    mGoldenImagePath.append("golden.ppm");
    testCaptureRequests(outputFormats, /* numRequests*/1, false);
}

// Test capture requests with RAW16.
TEST_F(HdrPlusClientTest, CaptureSingleRaw16) {
    std::vector<uint32_t> outputFormats = { HAL_PIXEL_FORMAT_RAW16 };
    testCaptureRequests(outputFormats, /* numRequests */1, false);
}

// Test capture requests with NV21.
TEST_F(HdrPlusClientTest, CaptureMultiYuv) {
    std::vector<uint32_t> outputFormats = { HAL_PIXEL_FORMAT_YCrCb_420_SP };
    char value[256];
    property_get("persist.hdrplus_client_test.num_requests", value,
                kDefaultNumRequests);
    int32_t numRequests = std::atoi(value);

    mVerifyOutput = property_get_bool("persist.hdrplus_client_test.verify_output", true);
    if (mVerifyOutput) {
        mGoldenImagePath.append(kBurstInputDir);
        mGoldenImagePath.append("golden.ppm");
    }
    testCaptureRequests(outputFormats, numRequests, true);
}

} // namespace android

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


