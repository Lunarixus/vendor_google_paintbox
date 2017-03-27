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
#ifndef PAINTBOX_HDR_PLUS_TYPES_H
#define PAINTBOX_HDR_PLUS_TYPES_H

#include <array>
#include <stdint.h>
#include <string>
#include <vector>

namespace pbcamera {

// This file defines the common types used in HDR+ client and HDR+ service API.

typedef int32_t status_t;

/*
 * ImageConfiguration and PlaneConfiguration define the layout of a buffer.
 * The following is an example of a NV21 buffer.
 *
 * <-------Y stride (in bytes)------->
 * <----width (in pixels)---->
 * Y Y Y Y Y Y Y Y Y Y Y Y Y Y . . . .  ^            ^
 * Y Y Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |            |
 * Y Y Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |            |
 * Y Y Y Y Y Y Y Y Y Y Y Y Y Y . . . .  height       Y scanline
 * Y Y Y Y Y Y Y Y Y Y Y Y Y Y . . . .  (in lines)   (in lines)
 * Y Y Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |            |
 * Y Y Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |            |
 * Y Y Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |            |
 * Y Y Y Y Y Y Y Y Y Y Y Y Y Y . . . .  |            |
 * Y Y Y Y Y Y Y Y Y Y Y Y Y Y . . . .  v            |
 * . . . . . . . . . . . . . . . . . .               |
 * . . . . . . . . . . . . . . . . . .               v
 * <------V/U stride (in bytes)------>
 * V U V U V U V U V U V U V U . . . .  ^
 * V U V U V U V U V U V U V U . . . .  |
 * V U V U V U V U V U V U V U . . . .  |
 * V U V U V U V U V U V U V U . . . .  V/U scanline
 * V U V U V U V U V U V U V U . . . .  (in lines)
 * . . . . . . . . . . . . . . . . . .  |
 * . . . . . . . . . . . . . . . . . .  v
 * . . . . . . . . . . . . . . . . . .  -> Image padding.
 */

// PlaneConfiguration defines an image planes configuration.
struct PlaneConfiguration {
    // Number of bytes in each line including padding.
    uint32_t stride;
    // Number of lines vertically including padding.
    uint32_t scanline;

    PlaneConfiguration() : stride(0), scanline(0) {};

    bool operator==(const PlaneConfiguration &other) const {
        return stride == other.stride &&
               scanline == other.scanline;
    }

    bool operator!=(const PlaneConfiguration &other) const {
        return !(*this == other);
    }
};

// ImageConfiguration defines an image configuration.
struct ImageConfiguration {
    // Image width.
    uint32_t width;
    // Image height;
    uint32_t height;
    // Image format;
    int format;
    // Configuration for each planes.
    std::vector<PlaneConfiguration> planes;
    // Number of padded bytes after the last plane.
    uint32_t padding;

    ImageConfiguration() : width(0), height(0), format(0), padding(0) {};

    bool operator==(const ImageConfiguration &other) const {
        return width == other.width &&
               height == other.height &&
               format == other.format &&
               planes == other.planes &&
               padding == other.padding;
    }

    bool operator!=(const ImageConfiguration &other) const {
        return !(*this == other);
    }
};

/*
 * StreamConfiguration defines a stream's configuration, such as its image buffer resolution, used
 * during stream configuration.
 */
struct StreamConfiguration {
    /*
     * Unique ID of the stream. Each stream must have an unique ID so it can be used to identify
     * the output streams of a StreamBuffer in CaptureRequest.
     */
    uint32_t id;

    // Image configuration.
    ImageConfiguration image;

    bool operator==(const StreamConfiguration &other) const {
        return id == other.id &&
               image == other.image;
    }

    bool operator!=(const StreamConfiguration &other) const {
        return !(*this == other);
    }
};

/*
 * SensorMode contains the sensor mode information.
 */
struct SensorMode {
    // Usually 0 is back camera and 1 is front camera.
    uint32_t cameraId;

    // Pixel array resolution.
    uint32_t pixelArrayWidth;
    uint32_t pixelArrayHeight;

    // Active array resolution.
    uint32_t activeArrayWidth;
    uint32_t activeArrayHeight;

    // Sensor output pixel clock.
    uint32_t outputPixelClkHz;

    // Sensor output format as defined in android_pixel_format.
    int format;

    SensorMode() : cameraId(0), pixelArrayWidth(0), pixelArrayHeight(0), activeArrayWidth(0),
                   activeArrayHeight(0), outputPixelClkHz(0) {};
};

/*
 * InputConfiguration defines the input configuration for HDR+ service.
 */
struct InputConfiguration {
    // Whether the input frames come from sensor MIPI or AP. If true, HDR+ service will get input
    // frames from sensor and sensorMode contains the sensor mode information. If false, HDR+
    // service will get input frames from AP and streamConfig contains the input stream
    // configuration.
    bool isSensorInput;
    // Sensor mode if isSensorInput is true.
    SensorMode sensorMode;
    // Input stream configuration if isSensorInput is false.
    StreamConfiguration streamConfig;

    InputConfiguration() : isSensorInput(false) {};
};

/*
 * StreamBuffer defines a buffer in a stream.
 */
struct StreamBuffer {
    // ID of the stream that this buffer belongs to.
    uint32_t streamId;
    //  DMA buffer fd for this buffer if it's an ION buffer.
    int32_t dmaBufFd;
    // Pointer to the data of this buffer.
    void* data;
    // Size of the allocated data.
    uint32_t dataSize;
};

/*
 * CaptureRequest defines a capture request that HDR+ client sends to HDR+ service.
 */
struct CaptureRequest {
    /*
     * ID of the capture request. Each capture request must have an unique ID. When HDR+ service
     * sends a CaptureResult to HDR+ client for this request, CaptureResult.requestId will be
     * assigned to this ID.
     */
    uint32_t id;
    /*
     * Output buffers of the request. The buffers will be filled with captured image when HDR+
     * service sends the output buffers in CaptureResult.
     */
    std::vector<StreamBuffer> outputBuffers;
};

// Util functions used in StaticMetadata and FrameMetadata.
namespace metadatautils {
template<typename T>
void appendValueToString(std::string *strOut, const char* key, T value);

template<typename T>
void appendVectorOrArrayToString(std::string *strOut, T values);

template<typename T>
void appendVectorOrArrayToString(std::string *strOut, const char* key, T values);

template<typename T, size_t SIZE>
void appendVectorArrayToString(std::string *strOut, const char* key,
            std::vector<std::array<T, SIZE>> values);

template<typename T, size_t SIZE>
void appendArrayArrayToString(std::string *strOut, const char* key,
            std::array<T, SIZE> values);
} // namespace metadatautils

/*
 * StaticMetadata defines a camera device's characteristics.
 *
 * If this structure is changed, serialization in MessengerToHdrPlusService and deserialization in
 * MessengerListenerFromHdrPlusClient should also be updated.
 */
struct StaticMetadata {
    // The following are from Android Camera Metadata
    uint8_t flashInfoAvailable; // android.flash.info.available
    std::array<int32_t, 2> sensitivityRange; // android.sensor.info.sensitivityRange
    int32_t maxAnalogSensitivity; // android.sensor.maxAnalogSensitivity
    std::array<int32_t, 2> pixelArraySize; // android.sensor.info.pixelArraySize
    std::array<int32_t, 4> activeArraySize; // android.sensor.info.activeArraySize
    std::vector<std::array<int32_t, 4>> opticalBlackRegions; // android.sensor.opticalBlackRegions
    // android.scaler.availableStreamConfigurations
    std::vector<std::array<int32_t, 4>> availableStreamConfigurations;
    uint8_t referenceIlluminant1; // android.sensor.referenceIlluminant1
    uint8_t referenceIlluminant2; // android.sensor.referenceIlluminant2
    std::array<float, 9> calibrationTransform1; // android.sensor.calibrationTransform1
    std::array<float, 9> calibrationTransform2; // android.sensor.calibrationTransform2
    std::array<float, 9> colorTransform1; // android.sensor.colorTransform1
    std::array<float, 9> colorTransform2; // android.sensor.colorTransform2
    int32_t whiteLevel; // android.sensor.info.whiteLevel
    uint8_t colorFilterArrangement; // android.sensor.info.colorFilterArrangement
    std::vector<float> availableApertures; // android.lens.info.availableApertures
    std::vector<float> availableFocalLengths; // android.lens.info.availableFocalLengths
    std::array<int32_t, 2> shadingMapSize; // android.lens.info.shadingMapSize
    uint8_t focusDistanceCalibration; // android.lens.info.focusDistanceCalibration

    // Check if the contents of lhs and rhs are equal. For vector and array variables, two are
    // equal if their elements are equal at the same position.
    bool operator==(const StaticMetadata& rhs) const {
        return flashInfoAvailable == rhs.flashInfoAvailable &&
               sensitivityRange == rhs.sensitivityRange &&
               maxAnalogSensitivity == rhs.maxAnalogSensitivity &&
               pixelArraySize == rhs.pixelArraySize &&
               activeArraySize == rhs.activeArraySize &&
               opticalBlackRegions == rhs.opticalBlackRegions &&
               availableStreamConfigurations == rhs.availableStreamConfigurations &&
               referenceIlluminant1 == rhs.referenceIlluminant1 &&
               referenceIlluminant2 == rhs.referenceIlluminant2 &&
               calibrationTransform1 == rhs.calibrationTransform1 &&
               calibrationTransform2 == rhs.calibrationTransform2 &&
               colorTransform1 == rhs.colorTransform1 &&
               colorTransform2 == rhs.colorTransform2 &&
               whiteLevel == rhs.whiteLevel &&
               colorFilterArrangement == rhs.colorFilterArrangement &&
               availableApertures == rhs.availableApertures &&
               availableFocalLengths == rhs.availableFocalLengths &&
               shadingMapSize == rhs.shadingMapSize &&
               focusDistanceCalibration == rhs.focusDistanceCalibration;
    }

    // Convert this static metadata to a string and append it to the specified string.
    void appendToString(std::string *strOut) const {
        if (strOut == nullptr) return;

        metadatautils::appendValueToString(strOut, "flashInfoAvailable", flashInfoAvailable);
        metadatautils::appendVectorOrArrayToString(strOut, "sensitivityRange", sensitivityRange);
        metadatautils::appendValueToString(strOut, "maxAnalogSensitivity", maxAnalogSensitivity);
        metadatautils::appendVectorOrArrayToString(strOut, "pixelArraySize", pixelArraySize);
        metadatautils::appendVectorOrArrayToString(strOut, "activeArraySize", activeArraySize);
        metadatautils::appendVectorArrayToString(strOut, "opticalBlackRegions",
                opticalBlackRegions);
        metadatautils::appendVectorArrayToString(strOut, "availableStreamConfigurations",
                availableStreamConfigurations);
        metadatautils::appendValueToString(strOut, "referenceIlluminant1", referenceIlluminant1);
        metadatautils::appendValueToString(strOut, "referenceIlluminant2", referenceIlluminant2);
        metadatautils::appendVectorOrArrayToString(strOut, "calibrationTransform1",
                calibrationTransform1);
        metadatautils::appendVectorOrArrayToString(strOut, "calibrationTransform2",
                calibrationTransform2);
        metadatautils::appendVectorOrArrayToString(strOut, "colorTransform1", colorTransform1);
        metadatautils::appendVectorOrArrayToString(strOut, "colorTransform2", colorTransform2);
        metadatautils::appendValueToString(strOut, "whiteLevel", whiteLevel);
        metadatautils::appendValueToString(strOut, "colorFilterArrangement",
                colorFilterArrangement);
        metadatautils::appendVectorOrArrayToString(strOut, "availableApertures",
                availableApertures);
        metadatautils::appendVectorOrArrayToString(strOut, "availableFocalLengths",
                availableFocalLengths);
        metadatautils::appendVectorOrArrayToString(strOut, "shadingMapSize", shadingMapSize);
        metadatautils::appendValueToString(strOut, "focusDistanceCalibration",
                focusDistanceCalibration);
    }
};

/*
 * FrameMetadata defines properties of a frame captured on AP.
 *
 * If this structure is changed, serialization in MessengerToHdrPlusService and deserialization in
 * MessengerListenerFromHdrPlusClient should also be updated.
 */
struct FrameMetadata {
    int64_t easelTimestamp; // Easel timestamp

    // The following are from Android Camera Metadata
    int64_t exposureTime; // android.sensor.exposureTime
    int32_t sensitivity; // android.sensor.sensitivity
    int32_t postRawSensitivityBoost; // android.control.postRawSensitivityBoost
    uint8_t flashMode; // android.flash.mode
    std::array<float, 4> colorCorrectionGains; // android.colorCorrection.gains
    std::array<float, 9> colorCorrectionTransform; // android.colorCorrection.transform
    std::array<float, 3> neutralColorPoint; // android.sensor.neutralColorPoint
    int64_t timestamp; // android.sensor.timestamp
    uint8_t blackLevelLock; // android.blackLevel.lock
    uint8_t faceDetectMode; // android.statistics.faceDetectMode
    std::vector<int32_t> faceIds; // android.statistics.faceIds
    std::vector<std::array<int32_t, 6>> faceLandmarks; // android.statistics.faceLandmarks
    std::vector<std::array<int32_t, 4>> faceRectangles; // android.statistics.faceRectangles
    std::vector<uint8_t> faceScores; // android.statistics.faceScores
    uint8_t sceneFlicker; // android.statistics.sceneFlicker
    std::array<std::array<double, 2>, 4> noiseProfile; // android.sensor.noiseProfile
    std::array<float, 4> dynamicBlackLevel; // android.sensor.dynamicBlackLevel
    std::vector<float> lensShadingMap; // android.statistics.lensShadingMap
    float focusDistance; // android.lens.focusDistance

    // Check if the contents of lhs and rhs are equal. For vector and array variables, two are
    // equal if their elements are equal at the same position.
    bool operator==(const FrameMetadata& rhs) const {
        return easelTimestamp == rhs.easelTimestamp &&
               exposureTime == rhs.exposureTime &&
               sensitivity == rhs.sensitivity &&
               postRawSensitivityBoost == rhs.postRawSensitivityBoost &&
               flashMode == rhs.flashMode &&
               colorCorrectionGains == rhs.colorCorrectionGains &&
               colorCorrectionTransform == rhs.colorCorrectionTransform &&
               neutralColorPoint == rhs.neutralColorPoint &&
               timestamp == rhs.timestamp &&
               blackLevelLock == rhs.blackLevelLock &&
               faceDetectMode == rhs.faceDetectMode &&
               faceIds == rhs.faceIds &&
               faceLandmarks == rhs.faceLandmarks &&
               faceRectangles == rhs.faceRectangles &&
               faceScores == rhs.faceScores &&
               sceneFlicker == rhs.sceneFlicker &&
               noiseProfile == rhs.noiseProfile &&
               dynamicBlackLevel == rhs.dynamicBlackLevel &&
               lensShadingMap == rhs.lensShadingMap &&
               focusDistance == rhs.focusDistance;
    }

    // Convert this static metadata to a string and append it to the specified string.
    void appendToString(std::string *strOut) const {
        if (strOut == nullptr) return;

        metadatautils::appendValueToString(strOut, "easelTimestamp", easelTimestamp);
        metadatautils::appendValueToString(strOut, "exposureTime", exposureTime);
        metadatautils::appendValueToString(strOut, "sensitivity", sensitivity);
        metadatautils::appendValueToString(strOut, "postRawSensitivityBoost",
                postRawSensitivityBoost);
        metadatautils::appendValueToString(strOut, "flashMode", flashMode);
        metadatautils::appendVectorOrArrayToString(strOut, "colorCorrectionGains",
                colorCorrectionGains);
        metadatautils::appendVectorOrArrayToString(strOut, "colorCorrectionTransform",
                colorCorrectionTransform);
        metadatautils::appendVectorOrArrayToString(strOut, "neutralColorPoint", neutralColorPoint);
        metadatautils::appendValueToString(strOut, "timestamp", timestamp);
        metadatautils::appendValueToString(strOut, "blackLevelLock", blackLevelLock);
        metadatautils::appendValueToString(strOut, "faceDetectMode", faceDetectMode);
        metadatautils::appendVectorOrArrayToString(strOut, "faceIds", faceIds);
        metadatautils::appendVectorArrayToString(strOut, "faceLandmarks", faceLandmarks);
        metadatautils::appendVectorArrayToString(strOut, "faceRectangles", faceRectangles);
        metadatautils::appendVectorOrArrayToString(strOut, "faceScores", faceScores);
        metadatautils::appendArrayArrayToString(strOut, "noiseProfile", noiseProfile);
        metadatautils::appendValueToString(strOut, "sceneFlicker", sceneFlicker);
        metadatautils::appendVectorOrArrayToString(strOut, "dynamicBlackLevel", dynamicBlackLevel);
        metadatautils::appendVectorOrArrayToString(strOut, "lensShadingMap", lensShadingMap);
        metadatautils::appendValueToString(strOut, "focusDistance", focusDistance);
    }
};

/*
 * ResultMetadata defines a process frame's properties that have been modified due to processing.
 *
 * If this structure is changed, serialization in MessengerToHdrPlusClient and deserialization in
 * MessengerListenerFromHdrPlusService should also be updated.
 */
struct ResultMetadata {
    int64_t easelTimestamp; // Easel timestamp of SOF of the base frame.
    int64_t timestamp; // android.sensor.timestamp. AP timestamp of exposure start of the base
                       // frame.

    // Check if the contents of lhs and rhs are equal. For vector and array variables, two are
    // equal if their elements are equal at the same position.
    bool operator==(const ResultMetadata& rhs) const {
        return easelTimestamp == rhs.easelTimestamp &&
               timestamp == rhs.timestamp;
    }

    // Convert this static metadata to a string and append it to the specified string.
    void appendToString(std::string *strOut) const {
        if (strOut == nullptr) return;
        metadatautils::appendValueToString(strOut, "easelTimestamp", easelTimestamp);
        metadatautils::appendValueToString(strOut, "timestamp", timestamp);
    }
};

/*
 * CaptureResult defines a capture result that HDR+ service returns to HDR+ client.
 */
struct CaptureResult {
    /*
     * ID of the CaptureRequest that this capture result corresponds to. It can be used to match
     * the original CaptureRequest when the HDR+ client receives this result.
     */
    uint32_t requestId;
    /*
     * Output buffers filled with processed frame by HDR+ service.
     */
    std::vector<StreamBuffer> outputBuffers;

    /*
     * Result metadata including modified properties due to processing.
     */
    ResultMetadata metadata;
};

// Util functions used in StaticMetadata and FrameMetadata.
namespace metadatautils {

/*
 * Append a key and a value to a string.
 *
 * strOut is the string to append a key and a value to.
 * key is the name of the data.
 * value is the value of the data.
 */
template<typename T>
void appendValueToString(std::string *strOut, const char* key, T value) {
    if (strOut == nullptr) return;
    (*strOut) += std::string(key) + ": " + std::to_string(value) + "\n";
}

/*
 * Append a vector or an array of values to a string.
 *
 * strOut is the string to append a key and values to.
 * values is a vector or an array containing values to append to the string.
 */
template<typename T>
void appendVectorOrArrayToString(std::string *strOut, T values) {
    if (strOut == nullptr) return;
    for (size_t i = 0; i < values.size(); i++) {
        (*strOut) += std::to_string(values[i]);
        if (i != values.size() - 1) {
            (*strOut) +=", ";
        }
    }
}

/*
 * Append a key and a vector or an array of values to a string.
 *
 * strOut is the string to append a key and values to.
 * key is the name of the data.
 * values is a vector or an array containing values to append to the string.
 */
template<typename T>
void appendVectorOrArrayToString(std::string *strOut, const char* key, T values) {
    if (strOut == nullptr) return;
    (*strOut) += std::string(key) + ": ";
    appendVectorOrArrayToString(strOut, values);
    (*strOut) += "\n";
}

/*
 * Append a key and a vector of arrays to a string.
 *
 * strOut is the string to append a key and values to.
 * key is the name of the data.
 * values is a vector of arrays containing values to append to the string.
 */
template<typename T, size_t SIZE>
void appendVectorArrayToString(std::string *strOut, const char* key,
            std::vector<std::array<T, SIZE>> values) {
    if (strOut == nullptr) return;
    (*strOut) += std::string(key) + ": ";
    for (size_t i = 0; i < values.size(); i++) {
        appendVectorOrArrayToString(strOut, values[i]);
        if (i != values.size() - 1) {
            (*strOut) +=", ";
        }
    }
    (*strOut) += "\n";
}

/*
 * Append a key and an array of arrays to a string.
 *
 * strOut is the string to append a key and values to.
 * key is the name of the data.
 * values is an array of arrays containing values to append to the string.
 */
template<typename T, size_t SIZE>
void appendArrayArrayToString(std::string *strOut, const char* key,
            std::array<T, SIZE> values) {
    if (strOut == nullptr) return;
    (*strOut) += std::string(key) + ": ";
    for (size_t i = 0; i < values.size(); i++) {
        appendVectorOrArrayToString(strOut, values[i]);
        if (i != values.size() - 1) {
            (*strOut) +=", ";
        }
    }
    (*strOut) += "\n";
}

} // namespace metadatautils

} // namespace pbcamera

#endif // PAINTBOX_HDR_PLUS_TYPES_H
