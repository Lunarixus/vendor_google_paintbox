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
#define LOG_TAG "HdrPlusTestBurstInput"
#include <utils/Log.h>

#include <algorithm>
#include <dirent.h>
#include <fstream>
#include <inttypes.h>
#include <string>
#include <sys/types.h>

#include "dng_file_stream.h"
#include "dng_host.h"
#include "dng_image.h"
#include "dng_info.h"
#include "dng_stream.h"
#include "dng_negative.h"

#include "HdrPlusTestBurstInput.h"

namespace android {

// Update value of the tag in metadata
#define RETURN_ON_METADATA_UPDATE_ERROR(_metadata, _tag, _var) \
    do { \
        status_t res = (_metadata)->update((_tag), &(_var), 1); \
        if (res != 0) { \
            ALOGE("%s: Updating tag 0x%x failed: %s (%d)", __FUNCTION__, (_tag), \
                    strerror(-res), res); \
            return res; \
        } \
    } while(0)

// Update an array of values of the tag in metadata
#define RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(_metadata, _tag, _vector) \
    do { \
        status_t res = (_metadata)->update((_tag), (_vector).data(), (_vector).size()); \
        if (res != 0) { \
            ALOGE("%s: Updating tag 0x%x failed: %s (%d)", __FUNCTION__, (_tag), \
                    strerror(-res), res); \
            return res; \
        } \
    } while(0)


HdrPlusTestBurstInput::HdrPlusTestBurstInput(const std::string &dir) : mDir(dir) {
    // Append '/' to the end of directory if it doesn't exist.
    if (mDir[mDir.length() - 1] != '/') {
        mDir += "/";
    }
    findAllDngFilenames();
}

HdrPlusTestBurstInput::~HdrPlusTestBurstInput() {
}

void HdrPlusTestBurstInput::findAllDngFilenames() {
    static const std::string kDngFileExt = ".dng";

    DIR *dir = ::opendir(mDir.data());
    if (dir == nullptr) {
        ALOGE("%s: Cannot open directory: %s", __FUNCTION__, mDir.data());
        return;
    }

    // Find all filenames that end with ".dng"
    struct dirent *d;
    while ((d = ::readdir(dir)) != nullptr) {
        std::string filename = d->d_name;
        if (filename.length() < kDngFileExt.length())
            continue;

        // Get the possible file extension.
        std::string ext = filename.substr(filename.length() - kDngFileExt.length());
        // Convert to lower case.
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        // Check if it is DNG file extension.
        if (ext == kDngFileExt) {
            ALOGV("%s: Found DNG file: %s%s", __FUNCTION__, mDir.data(), filename.data());
            mDngFilenames.push_back(mDir + filename);
        }
    }

    ::closedir(dir);

    // Sort the filenames in mDngFilenames so the position is the frame number.
    std::sort(mDngFilenames.begin(), mDngFilenames.end());
}

uint32_t HdrPlusTestBurstInput::getNumberOfBurstInputs() {
    return mDngFilenames.size();
}

status_t HdrPlusTestBurstInput::extractEntries(std::vector<std::string> *entries, std::string line,
        const char *delimiters) {
    if (entries == nullptr) return BAD_VALUE;

    const std::string kDefaultDelimiters(" []()/");
    if (delimiters == nullptr) {
        delimiters = kDefaultDelimiters.data();
    }

    // Extract entries.
    char *savePtr = nullptr;
    char *entry = ::strtok_r(&line[0], delimiters, &savePtr);
    while (entry != nullptr) {
        entries->push_back(entry);
        entry = ::strtok_r(nullptr, delimiters, &savePtr);
    }

    return entries->size() > 0 ? OK : BAD_VALUE;
}

std::string::size_type HdrPlusTestBurstInput::getNumEntriesFromLine(const std::string &keyLine) {
    // Get the number of entires
    std::string::size_type n = keyLine.find_last_of("[");
    if (n == std::string::npos) {
        // Couldn't find the number of entries.
        return n;
    }

    return std::stoul(keyLine.substr(n + 1));
}

status_t HdrPlusTestBurstInput::loadInt32Metadata(std::ifstream *infile,
        const std::string &keyLine, CameraMetadata *metadata, uint32_t tag) {
    if (infile == nullptr || metadata == nullptr) return BAD_VALUE;

    std::string::size_type numValues = getNumEntriesFromLine(keyLine);
    if (numValues == 0) {
        // Nothing to do if the number of value is 0.
        return OK;
    }

    std::vector<int32_t> values;
    std::string line;

    do {
        if (!std::getline(*infile, line)) {
            ALOGE("%s: Cannot find the value for %s", __FUNCTION__, line.data());
            return BAD_VALUE;
        }

        // Extract all entries.
        std::vector<std::string> entries;
        status_t res = extractEntries(&entries, line);
        if (res != OK) {
            ALOGE("%s: Failed to extract entries from line: %s.", __FUNCTION__, line.data());
            return BAD_VALUE;
        }

        // Extract int32_t from entries.
        for (auto entry : entries) {
            int32_t i = std::stol(entry);
            values.push_back(i);
        }

        // If numValues is valid, values may span multiple lines.
    } while (numValues != std::string::npos && values.size() != numValues);

    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, tag, values);
    return OK;
}

status_t HdrPlusTestBurstInput::loadInt64Metadata(std::ifstream *infile,
        const std::string &keyLine, CameraMetadata *metadata, uint32_t tag) {
    if (infile == nullptr || metadata == nullptr) return BAD_VALUE;

    std::string::size_type numValues = getNumEntriesFromLine(keyLine);
    if (numValues == 0) {
        // Nothing to do if the number of value is 0.
        return OK;
    }

    std::vector<int64_t> values;
    std::string line;

    do {
        if (!std::getline(*infile, line)) {
            ALOGE("%s: Cannot find the value for %s", __FUNCTION__, line.data());
            return BAD_VALUE;
        }

        // Extract all entries.
        std::vector<std::string> entries;
        status_t res = extractEntries(&entries, line);
        if (res != OK) {
            ALOGE("%s: Failed to extract entries from line: %s.", __FUNCTION__, line.data());
            return BAD_VALUE;
        }

        // Extract int32_t from entries.
        for (auto entry : entries) {
            int64_t i = std::stoll(entry);
            values.push_back(i);
        }

        // If numValues is valid, values may span multiple lines.
    } while (numValues != std::string::npos && values.size() != numValues);

    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, tag, values);
    return OK;
}

status_t HdrPlusTestBurstInput::loadByteMetadata(std::ifstream *infile,
        const std::string &keyLine, CameraMetadata *metadata, uint32_t tag) {
    if (infile == nullptr || metadata == nullptr) return BAD_VALUE;

    std::string::size_type numValues = getNumEntriesFromLine(keyLine);
    if (numValues == 0) {
        // Nothing to do if the number of value is 0.
        return OK;
    }

    std::vector<uint8_t> values;
    std::string line;

    do {
        if (!std::getline(*infile, line)) {
            ALOGE("%s: Cannot find the value for %s", __FUNCTION__, line.data());
            return BAD_VALUE;
        }

        // Extract all entries.
        std::vector<std::string> entries;
        status_t res = extractEntries(&entries, line);
        if (res != OK) {
            ALOGE("%s: Failed to extract entries from line: %s.", __FUNCTION__, line.data());
            return BAD_VALUE;
        }

        // Extract uint8_t from entries.
        for (auto entry : entries) {
            uint8_t i = std::stoi(entry);
            values.push_back(i);
        }

        // If numValues is valid, values may span multiple lines.
    } while (numValues != std::string::npos && values.size() != numValues);

    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, tag, values);
    return OK;
}

status_t HdrPlusTestBurstInput::loadFloats(std::ifstream *infile,
        const std::string &keyLine, const char *delimiters, std::vector<float> *values) {
    if (infile == nullptr || values == nullptr) return BAD_VALUE;

    std::string::size_type numValues = getNumEntriesFromLine(keyLine);
    if (numValues == 0) {
        // Nothing to do if the number of value is 0.
        return OK;
    }

    values->resize(0);
    std::string line;

    do {
        if (!std::getline(*infile, line)) {
            ALOGE("%s: Cannot find the value for %s", __FUNCTION__, line.data());
            return BAD_VALUE;
        }

        // Extract all entries.
        std::vector<std::string> entries;
        status_t res = extractEntries(&entries, line, delimiters);
        if (res != OK) {
            ALOGE("%s: Failed to extract entries from line: %s.", __FUNCTION__, line.data());
            return BAD_VALUE;
        }

        // Extract float from entries.
        for (auto entry : entries) {
            float i = std::stof(entry);
            values->push_back(i);
        }

        // If numValues is valid, values may span multiple lines.
    } while (numValues != std::string::npos && values->size() != numValues);
    return OK;
}

status_t HdrPlusTestBurstInput::loadFloatMetadata(std::ifstream *infile,
        const std::string &keyLine, CameraMetadata *metadata, uint32_t tag,
        const char *delimiters) {
    if (infile == nullptr || metadata == nullptr) return BAD_VALUE;

    std::vector<float> values;

    status_t res = loadFloats(infile, keyLine, delimiters, &values);
    if (res != OK) {
        ALOGE("%s: Load floats failed: %s (%d)", __FUNCTION__, strerror(-res), res);
        return res;
    }

    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, tag, values);
    return OK;
}

status_t HdrPlusTestBurstInput::loadDoubleMetadata(std::ifstream *infile,
        const std::string &keyLine, CameraMetadata *metadata, uint32_t tag,
        const char *delimiters) {
    if (infile == nullptr || metadata == nullptr) return BAD_VALUE;

    std::string::size_type numValues = getNumEntriesFromLine(keyLine);
    if (numValues == 0) {
        // Nothing to do if the number of value is 0.
        return OK;
    }

    std::vector<double> values;
    std::string line;

    do {
        if (!std::getline(*infile, line)) {
            ALOGE("%s: Cannot find the value for %s", __FUNCTION__, line.data());
            return BAD_VALUE;
        }

        // Extract all entries.
        std::vector<std::string> entries;
        status_t res = extractEntries(&entries, line, delimiters);
        if (res != OK) {
            ALOGE("%s: Failed to extract entries from line: %s.", __FUNCTION__, line.data());
            return BAD_VALUE;
        }

        // Extract float from entries.
        for (auto entry : entries) {
            double i = std::stod(entry);
            values.push_back(i);
        }

        // If numValues is valid, values may span multiple lines.
    } while (numValues != std::string::npos && values.size() != numValues);

    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, tag, values);
    return OK;
}

status_t HdrPlusTestBurstInput::loadRationalMetadata(std::ifstream *infile,
        const std::string &keyLine, CameraMetadata *metadata, uint32_t tag,
        const char *delimiters) {
    if (infile == nullptr || metadata == nullptr) return BAD_VALUE;

    std::string::size_type numValues = getNumEntriesFromLine(keyLine);
    if (numValues == 0) {
        // Nothing to do if the number of value is 0.
        return OK;
    }

    std::vector<camera_metadata_rational_t> values;
    std::string line;

    do {
        if (!std::getline(*infile, line)) {
            ALOGE("%s: Cannot find the value for %s", __FUNCTION__, line.data());
            return BAD_VALUE;
        }

        // Extract all entries.
        std::vector<std::string> entries;
        status_t res = extractEntries(&entries, line, delimiters);
        if (res != OK) {
            ALOGE("%s: Failed to extract entries from line: %s.", __FUNCTION__, line.data());
            return BAD_VALUE;
        }

        if (entries.size() % 2 != 0) {
            ALOGE("%s: Failed to extract even number of entries from line: %s.",
                    __FUNCTION__, line.data());
            return BAD_VALUE;
        }

        // Extract rationals from entries.
        for (size_t i = 0; i < entries.size(); i += 2) {
            camera_metadata_rational_t r;
            r.numerator = std::stol(entries[i]);
            r.denominator = std::stol(entries[i + 1]);
            values.push_back(r);
        }

        // If numValues is valid, values may span multiple lines.
    } while (numValues != std::string::npos && values.size() != numValues);

    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, tag, values);
    return OK;
}

status_t HdrPlusTestBurstInput::loadLensShadingMap(std::ifstream *infile,
        const std::string &keyLine, CameraMetadata *metadata) {
    if (infile == nullptr || metadata == nullptr) return BAD_VALUE;

    std::vector<float> values;
    status_t res = loadFloats(infile, keyLine, "LensShadigMapRGvoB{}()[] ,_:", &values);
    if (res != OK) {
        ALOGE("%s: Load floats failed: %s (%d)", __FUNCTION__, strerror(-res), res);
        return res;
    }

    // Convert the lens shading map order.
    // Lens shading map dumped in camera app is grouped by channels, i.e. "all gains for R",
    // followed by "all gains for G_even", followed by "all gains for G_odd", followed by
    // "all gains for B".
    // android.statistics.lensShadingMap is grouped by pixels, i.e. [R, G_even, G_odd, B] of pixel
    // 0, followed by [R, G_even, G_odd, B] of pixel 1.
    std::vector<float> lensShadingMap;

    lensShadingMap.resize(values.size());
    uint32_t entriesPerChannel = values.size() / 4;
    for (uint32_t i = 0; i < lensShadingMap.size(); i++) {
        uint32_t pixel = i / 4;
        uint32_t channel = i % 4;
        lensShadingMap[i] = values[pixel + channel * entriesPerChannel];
    }

    RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata, ANDROID_STATISTICS_LENS_SHADING_MAP,
        lensShadingMap);
    return OK;
}

status_t HdrPlusTestBurstInput::loadStaticMetadataFromFile(CameraMetadata *metadata) {
    if (metadata == nullptr) {
        ALOGE("%s: metadata cannot be null.", __FUNCTION__);
        return BAD_VALUE;
    }

    static const std::string kMetadataFilename = "/static_metadata_hal3.txt";

    std::string filename = mDir + kMetadataFilename;
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        ALOGE("%s: Cannot open file: %s", __FUNCTION__, filename.data());
        return BAD_VALUE;
    }

    status_t res;
    std::string line;
    std::string::size_type n = 0;

    // Parse each line.
    while(std::getline(infile, line)) {
        if ((n = line.find("android.flash.info.available")) != std::string::npos) {
            if (!std::getline(infile, line)) {
                ALOGE("%s: Cannot find the value for %s", __FUNCTION__, line.data());
                return BAD_VALUE;
            }

            uint8_t flashInfoAvailable = line.find("TRUE") == std::string::npos ?
                    ANDROID_FLASH_INFO_AVAILABLE_FALSE : ANDROID_FLASH_INFO_AVAILABLE_TRUE;
            RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_FLASH_INFO_AVAILABLE,
                    flashInfoAvailable);
        } else if ((n = line.find("android.sensor.info.sensitivityRange")) != std::string::npos) {
            res = loadInt32Metadata(&infile, line, metadata, ANDROID_SENSOR_INFO_SENSITIVITY_RANGE);
            if (res != OK) {
                ALOGE("%s: Parsing sensitivityRange failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.sensor.maxAnalogSensitivity")) != std::string::npos) {
            res = loadInt32Metadata(&infile, line, metadata, ANDROID_SENSOR_MAX_ANALOG_SENSITIVITY);
            if (res != OK) {
                ALOGE("%s: Parsing maxAnalogSensitivity failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.sensor.info.pixelArraySize")) != std::string::npos) {
            res = loadInt32Metadata(&infile, line, metadata, ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE);
            if (res != OK) {
                ALOGE("%s: Parsing pixelArraySize failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.sensor.info.activeArraySize")) != std::string::npos) {
            res = loadInt32Metadata(&infile, line, metadata, ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE);
            if (res != OK) {
                ALOGE("%s: Parsing activeArraySize failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.sensor.opticalBlackRegions")) != std::string::npos) {
            res = loadInt32Metadata(&infile, line, metadata, ANDROID_SENSOR_OPTICAL_BLACK_REGIONS);
            if (res != OK) {
                ALOGE("%s: Parsing opticalBlackRegions failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.scaler.availableStreamConfigurations")) !=
                std::string::npos) {
            int32_t numEntries = getNumEntriesFromLine(line);
            std::vector<int32_t> availableStreamConfigurations;
            while (static_cast<int32_t>(availableStreamConfigurations.size()) < numEntries) {
                if (!std::getline(infile, line)) {
                    ALOGE("%s: Cannot find the value for %s", __FUNCTION__, line.data());
                    return BAD_VALUE;
                }

                // Extract all entries.
                std::vector<std::string> entries;
                status_t res = extractEntries(&entries, line);
                if (res != OK) {
                    ALOGE("%s: Failed to extract entries from line: %s.", __FUNCTION__,
                            line.data());
                    return BAD_VALUE;
                }

                for (auto entry : entries) {
                    if (entry.find("INPUT") != std::string::npos) {
                        availableStreamConfigurations.push_back(
                                ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT);
                    } else if (entry.find("OUTPUT") != std::string::npos) {
                        availableStreamConfigurations.push_back(
                                ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT);
                    } else {
                        int32_t i = std::stol(entry);
                        availableStreamConfigurations.push_back(i);
                    }
                }
            }

            RETURN_ON_ARRAY_METADATA_UPDATE_ERROR(metadata,
                    ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, availableStreamConfigurations);
        } else if ((n = line.find("android.sensor.referenceIlluminant1")) != std::string::npos) {
            res = loadByteMetadata(&infile, line, metadata, ANDROID_SENSOR_REFERENCE_ILLUMINANT1);
            if (res != OK) {
                ALOGE("%s: Parsing referenceIlluminant1 failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.sensor.referenceIlluminant2")) != std::string::npos) {
            res = loadByteMetadata(&infile, line, metadata, ANDROID_SENSOR_REFERENCE_ILLUMINANT2);
            if (res != OK) {
                ALOGE("%s: Parsing referenceIlluminant1 failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.sensor.calibrationTransform1")) != std::string::npos) {
            res = loadRationalMetadata(&infile, line, metadata,
                    ANDROID_SENSOR_CALIBRATION_TRANSFORM1);
            if (res != OK) {
                ALOGE("%s: Parsing calibrationTransform1 failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.sensor.calibrationTransform2")) != std::string::npos) {
            res = loadRationalMetadata(&infile, line, metadata,
                    ANDROID_SENSOR_CALIBRATION_TRANSFORM2);
            if (res != OK) {
                ALOGE("%s: Parsing calibrationTransform2 failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.sensor.colorTransform1")) != std::string::npos) {
            res = loadRationalMetadata(&infile, line, metadata, ANDROID_SENSOR_COLOR_TRANSFORM1);
            if (res != OK) {
                ALOGE("%s: Parsing colorTransform1 failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.sensor.colorTransform2")) != std::string::npos) {
            res = loadRationalMetadata(&infile, line, metadata, ANDROID_SENSOR_COLOR_TRANSFORM2);
            if (res != OK) {
                ALOGE("%s: Parsing colorTransform1 failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.sensor.info.whiteLevel")) != std::string::npos) {
            res = loadInt32Metadata(&infile, line, metadata, ANDROID_SENSOR_INFO_WHITE_LEVEL);
            if (res != OK) {
                ALOGE("%s: Parsing whiteLevel failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.sensor.info.colorFilterArrangement")) !=
                    std::string::npos) {
            if (!std::getline(infile, line)) {
                ALOGE("%s: Cannot find the value for %s", __FUNCTION__, line.data());
                return BAD_VALUE;
            }

            // Extract all entries.
            std::vector<std::string> entries;
            status_t res = extractEntries(&entries, line);
            if (res != OK) {
                ALOGE("%s: Failed to extract entries from line: %s.", __FUNCTION__, line.data());
                return BAD_VALUE;
            }

            // Extract uint8_t
            uint8_t colorFilterArrangement;
            if (entries[0].find("RGGB") != std::string::npos) {
                colorFilterArrangement = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGGB;
            } else if (entries[0].find("GRBG") != std::string::npos) {
                colorFilterArrangement = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GRBG;
            } else if (entries[0].find("GBRG") != std::string::npos) {
                colorFilterArrangement = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GBRG;
            } else if (entries[0].find("BGGR") != std::string::npos) {
                colorFilterArrangement = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_BGGR;
            } else if (entries[0].find("RGB") != std::string::npos) {
                colorFilterArrangement = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGB;
            } else {
                ALOGE("%s: colorFilterArrangement: %s not supported.", __FUNCTION__,
                    entries[0].data());
                return BAD_VALUE;
            }

            RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT,
                    colorFilterArrangement);
        } else if ((n = line.find("android.lens.info.availableApertures")) != std::string::npos) {
            res = loadFloatMetadata(&infile, line, metadata, ANDROID_LENS_INFO_AVAILABLE_APERTURES);
            if (res != OK) {
                ALOGE("%s: Parsing availableApertures failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.lens.info.availableFocalLengths")) !=
                std::string::npos) {
            res = loadFloatMetadata(&infile, line, metadata,
                    ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS);
            if (res != OK) {
                ALOGE("%s: Parsing availableFocalLengths failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.lens.info.shadingMapSize")) != std::string::npos) {
            res = loadInt32Metadata(&infile, line, metadata, ANDROID_LENS_INFO_SHADING_MAP_SIZE);
            if (res != OK) {
                ALOGE("%s: Parsing shadingMapSize failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.lens.info.focusDistanceCalibration")) !=
                    std::string::npos) {
            if (!std::getline(infile, line)) {
                ALOGE("%s: Cannot find the value for %s", __FUNCTION__, line.data());
                return BAD_VALUE;
            }

            // Parse focus distance calibration.
            uint8_t focusDistanceCalibration;
            if (line.find("CALIBRATED") != std::string::npos) {
                focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_CALIBRATED;
            } else if (line.find("UNCALIBRATED") != std::string::npos) {
                focusDistanceCalibration =
                        ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED;
            } else if (line.find("APPROXIMATE") != std::string::npos) {
                focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_APPROXIMATE;
            } else {
                ALOGE("%s: focusDistanceCalibration: %s not supported.", __FUNCTION__, line.data());
                return BAD_VALUE;
            }

            RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION,
                    focusDistanceCalibration);
        }
    }

    return OK;
}

status_t HdrPlusTestBurstInput::loadFrameMetadataFromFile(CameraMetadata *metadata,
        uint32_t frameNum, const std::string &filename) {
    if (metadata == nullptr) {
        ALOGE("%s: metadata cannot be null.", __FUNCTION__);
        return BAD_VALUE;
    }

    std::ifstream infile(filename);
    if (!infile.is_open()) {
        ALOGE("%s: Cannot open file: %s", __FUNCTION__, filename.data());
        return BAD_VALUE;
    }

    std::string line;
    const std::string resultFrameString = "Result frame";
    std::string::size_type n = 0;

    // Find the section for frameNum
    while(std::getline(infile, line)) {
        if ((n = line.find(resultFrameString)) != std::string::npos) {
            // Found a new frame.
            if (std::stoul(line.substr(n + resultFrameString.length())) == frameNum) {
                ALOGV("%s: Found metadata for frame %d", __FUNCTION__, frameNum);
                break;
            }
        }
    }

    status_t res;

    // Parse each line.
    while(std::getline(infile, line)) {
        if ((n = line.find(resultFrameString)) != std::string::npos) {
            // Return if found another frame's metadata.
            return OK;
        } else if ((n = line.find("android.sensor.exposureTime")) != std::string::npos) {
            res = loadInt64Metadata(&infile, line, metadata, ANDROID_SENSOR_EXPOSURE_TIME);
            if (res != OK) {
                ALOGE("%s: Parsing exposureTime failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.sensor.sensitivity")) != std::string::npos) {
            res = loadInt32Metadata(&infile, line, metadata, ANDROID_SENSOR_SENSITIVITY);
            if (res != OK) {
                ALOGE("%s: Parsing sensitivity failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.control.postRawSensitivityBoost")) !=
                std::string::npos) {
            res = loadInt32Metadata(&infile, line, metadata,
                    ANDROID_CONTROL_POST_RAW_SENSITIVITY_BOOST);
            if (res != OK) {
                ALOGE("%s: Parsing postRawSensitivityBoost failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.flash.mode")) != std::string::npos) {
            res = loadByteMetadata(&infile, line, metadata, ANDROID_FLASH_MODE);
            if (res != OK) {
                ALOGE("%s: Parsing flash mode failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.colorCorrection.gains")) != std::string::npos) {
            res = loadFloatMetadata(&infile, line, metadata, ANDROID_COLOR_CORRECTION_GAINS,
                    "RgbChanelVctorGvdB_:,/ {}");
            if (res != OK) {
                ALOGE("%s: Parsing colorCorrection gains failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.colorCorrection.transform")) != std::string::npos) {
            res = loadRationalMetadata(&infile, line, metadata, ANDROID_COLOR_CORRECTION_TRANSFORM,
                    "ColrSpaceTnsfm()[], /");
            if (res != OK) {
                ALOGE("%s: Parsing colorCorrection transform failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.sensor.neutralColorPoint")) != std::string::npos) {
            res = loadRationalMetadata(&infile, line, metadata, ANDROID_SENSOR_NEUTRAL_COLOR_POINT);
            if (res != OK) {
                ALOGE("%s: Parsing sensor neutralColorPoint failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.sensor.timestamp")) != std::string::npos) {
            res = loadInt64Metadata(&infile, line, metadata, ANDROID_SENSOR_TIMESTAMP);
            if (res != OK) {
                ALOGE("%s: Parsing sensor timestamp failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.blackLevel.lock")) != std::string::npos) {
            if (!std::getline(infile, line)) {
                ALOGE("%s: Cannot find the value for %s", __FUNCTION__, line.data());
                return BAD_VALUE;
            }

            uint8_t blackLevelLock = line.find("true") == std::string::npos ?
                    ANDROID_BLACK_LEVEL_LOCK_OFF : ANDROID_BLACK_LEVEL_LOCK_ON;
            RETURN_ON_METADATA_UPDATE_ERROR(metadata, ANDROID_BLACK_LEVEL_LOCK, blackLevelLock);
        } else if ((n = line.find("android.statistics.faceDetectMode")) != std::string::npos) {
            res = loadByteMetadata(&infile, line, metadata, ANDROID_STATISTICS_FACE_DETECT_MODE);
            if (res != OK) {
                ALOGE("%s: Parsing faceDetectMode failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.statistics.faceIds")) != std::string::npos) {
            ALOGE("%s: Parsing faceIds not implemented yet.", __FUNCTION__);
            return BAD_VALUE;
        } else if ((n = line.find("android.statistics.faceLandmarks")) != std::string::npos) {
            ALOGE("%s: Parsing faceLandmarks not implemented yet.", __FUNCTION__);
            return BAD_VALUE;
        } else if ((n = line.find("android.statistics.faceRectangles")) != std::string::npos) {
            ALOGE("%s: Parsing faceRectangles not implemented yet.", __FUNCTION__);
            return BAD_VALUE;
        } else if ((n = line.find("android.statistics.faceScores")) != std::string::npos) {
            ALOGE("%s: Parsing faceScores not implemented yet.", __FUNCTION__);
            return BAD_VALUE;
        } else if ((n = line.find("android.statistics.sceneFlicker")) != std::string::npos) {
            res = loadByteMetadata(&infile, line, metadata, ANDROID_STATISTICS_SCENE_FLICKER);
            if (res != OK) {
                ALOGE("%s: Parsing sceneFlicker failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.sensor.noiseProfile")) != std::string::npos) {
            res = loadDoubleMetadata(&infile, line, metadata, ANDROID_SENSOR_NOISE_PROFILE,
                    "Pair:/[], ");
            if (res != OK) {
                ALOGE("%s: Parsing noiseProfile failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.sensor.dynamicBlackLevel")) != std::string::npos) {
            res = loadFloatMetadata(&infile, line, metadata, ANDROID_SENSOR_DYNAMIC_BLACK_LEVEL);
            if (res != OK) {
                ALOGE("%s: Parsing dynamicBlackLevel failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.statistics.lensShadingCorrectionMap")) !=
                    std::string::npos) {
            res = loadLensShadingMap(&infile, line, metadata);
            if (res != OK) {
                ALOGE("%s: Parsing lensShadingCorrectionMap failed.", __FUNCTION__);
                return res;
            }
        } else if ((n = line.find("android.lens.focusDistance")) != std::string::npos) {
            res = loadFloatMetadata(&infile, line, metadata, ANDROID_LENS_FOCUS_DISTANCE);
            if (res != OK) {
                ALOGE("%s: Parsing focusDistance failed.", __FUNCTION__);
                return res;
            }
        }
    }

    return OK;
}

status_t HdrPlusTestBurstInput::convertRaw16ToRaw10(void *raw10Dst, size_t raw10Size,
            const std::vector<uint16_t> &raw16Src, uint16_t whiteLevel) {
    if (raw10Dst == nullptr) {
        ALOGE("%s: raw10Dst is null", __FUNCTION__);
        return BAD_VALUE;
    }

    if (raw10Size != raw16Src.size() / 8 * 10) {
        ALOGE("%s: raw16Src has %zu pixels but raw10Size is %zu.", __FUNCTION__, raw16Src.size(),
                raw10Size);
        return BAD_VALUE;
    }

    assert(whiteLevel > 0);
    uint8_t *raw10 = static_cast<uint8_t*>(raw10Dst);
    for (size_t i = 0; i < raw10Size; i += 5) {
        // Every 4 pixel in raw16Src are converted to 5 bytes in raw10.
        size_t p = i / 5 * 4; // Pixel in raw16Src
        raw10[i + 4] = 0; // Set the fifth byte to 0 so it can be OR'd later.
        for (size_t j = 0; j < 4; j++) {
            assert(raw16Src[p + j] <= whiteLevel);

            // Map a value from [0...whiteLevel] to [0...1023]
            uint32_t value = static_cast<uint32_t>(raw16Src[p + j]) * 1023 / whiteLevel;

            // First 4 bytes contain 8 MSB of each pixel.
            raw10[i + j] = value >> 2;
            // The fifth byte contains 2 LSB of each pixel.
            raw10[i + 4] |= (value & 0x3) << (j * 2);
        }
    }

    return OK;
}

status_t HdrPlusTestBurstInput::loadRaw10BufferFromFile(void *buffer, size_t bufferSize,
        const std::string &filename) {
    if (buffer == nullptr) {
        ALOGE("%s: buffer cannot be null.", __FUNCTION__);
        return BAD_VALUE;
    }

    // Open the DNG file.
    dng_host host;
    dng_file_stream stream(filename.data());
    std::unique_ptr<dng_negative> negative(host.Make_dng_negative());
    dng_info info;
    info.Parse(host, stream);
    info.PostParse(host);
    negative->Parse(host, stream, info);
    negative->PostParse(host, stream, info);
    negative->ReadStage1Image(host, stream, info);

    // Get the DNG image.
    const dng_image* image = negative->Stage1Image();
    uint32_t pixelType = image->PixelType();
    uint32_t width = image->Width();
    uint32_t height = image->Height();
    uint32_t numPlanes = image->Planes();

    if (pixelType != ttShort || numPlanes != 1) {
        ALOGE("%s: Only 16-bit bayer is supported: type %d, num planes %d.", __FUNCTION__,
                pixelType, numPlanes);
        return BAD_VALUE;
    }

    if (bufferSize != width * height * 10 / 8) {
        ALOGE("%s: DNG buffer size %d doesn't match the specified buffer size %d.", __FUNCTION__,
                width * height * 10 / 8, static_cast<int>(bufferSize));
        return BAD_VALUE;
    }

    // Create a temporary raw16 buffer to store pixel data of the DNG image.
    std::vector<uint16_t> raw16(width * height);

    // Create a pixelBuffer using the specified buffer and copy the pixel data from the DNG image.
    dng_pixel_buffer pixelBuffer(image->Bounds(), /*plane*/0, numPlanes, pixelType, pcInterleaved,
            raw16.data());
    image->Get(pixelBuffer);

    // Convert raw16 to raw10.
    status_t res = convertRaw16ToRaw10(buffer, bufferSize, raw16, negative->WhiteLevel());
    if (res != OK) {
        ALOGE("%s: Converting raw16 to raw10 failed: %s (%d)", __FUNCTION__, strerror(-res), res);
        return res;
    }

    return OK;
}

status_t HdrPlusTestBurstInput::loadRaw10BufferAndMetadataFromFile(void *buffer, size_t bufferSize,
        CameraMetadata *metadata, uint32_t frameNum) {
    if (frameNum >= mDngFilenames.size()) {
        ALOGE("%s: Frame number (%d) is invalid. There are only %d files.", __FUNCTION__,
                static_cast<int>(frameNum), static_cast<int>(mDngFilenames.size()));
        return BAD_VALUE;
    }

    // Load DNG file
    status_t res = loadRaw10BufferFromFile(buffer, bufferSize, mDngFilenames[frameNum]);
    if (res != OK) {
        ALOGE("%s: Failed to load buffer %d from file: %s. %s (%d)", __FUNCTION__,
                frameNum, mDngFilenames[frameNum].data(), strerror(-res), res);
        return res;
    }

    // Load frame metadata.
    static const std::string kMetadataFilename = "/payload_burst_actual_hal3.txt";
    res = loadFrameMetadataFromFile(metadata, frameNum, mDir + kMetadataFilename);
    if (res != OK) {
        ALOGE("%s: Failed to load metadata %d from file: %s. %s (%d)", __FUNCTION__,
                frameNum, (mDir + kMetadataFilename).data(), strerror(-res), res);
        return res;
    }

    return res;
}

} // namespace android
