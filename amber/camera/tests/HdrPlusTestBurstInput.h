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
#ifndef PAINTBOX_HDR_PLUS_CLIENT_TEST_BURST_INPUT_H
#define PAINTBOX_HDR_PLUS_CLIENT_TEST_BURST_INPUT_H

#include <CameraMetadata.h>

using ::android::hardware::camera::common::V1_0::helper::CameraMetadata;

namespace android {

/**
 * HdrPlusTestBurstInput
 *
 * HdrPlusTestBurstInput can be used to search for HDR+ burst input files and metadata files, and
 * load burst input buffers and metadata given a directory.
 *
 */
class HdrPlusTestBurstInput {
public:
    // dir is the directory where the HDR+ burst input files and metadata files are.
    HdrPlusTestBurstInput(const std::string &dir);
    virtual ~HdrPlusTestBurstInput();

    /*
     * Load static metadata from file found in the directory.
     *
     * metadata will be filled with static metadata parsed from a static metadata file.
     *
     * Returns:
     *  OK:         on success.
     *  BAD_VALUE:  if metadata is nullptr, or a static metadata file cannot be found, or it fails
     *              to parse static metadata.
     */
    status_t loadStaticMetadataFromFile(CameraMetadata *metadata);

    // Return the number of burst input files found in the directory.
    uint32_t getNumberOfBurstInputs();

    /*
     * Load a RAW10 buffer and its result metadata for the frame number.
     *
     * buffer is the buffer to be filled with burst input data.
     * bufferSize is the size of the buffer.
     * metadata is the metadata to be filled with result metadata.
     * frameNum is the frame number.
     *
     * Returns:
     *  OK:         on success.
     *  BAD_VALUE:  if buffer is nullptr, or bufferSize doesn't match the size of the input buffer
     *              loaded from the file, or metadata is nullptr, or frameNum is not smaller than
     *              the number of burst inputs, or it fails to parse result metadata.
     */
    status_t loadRaw10BufferAndMetadataFromFile(void *buffer, size_t bufferSize,
            CameraMetadata *metadata, uint32_t frameNum);

private:
    // Return the number of entries from keyLine (in format "[<numEntries>]").
    // Return std::string::npos if parsing failed.
    std::string::size_type getNumEntriesFromLine(const std::string &keyLine);

    // Load an array of int32 and set metadata with the tag and values.
    status_t loadInt32Metadata(std::ifstream *infile, const std::string &keyLine,
            CameraMetadata *metadata, uint32_t tag, const char *delimiters = nullptr);

    // Load an array of int64 and set metadata with the tag and values.
    status_t loadInt64Metadata(std::ifstream *infile, const std::string &keyLine,
            CameraMetadata *metadata, uint32_t tag);

    // Load an array of bytes and set metadata with the tag and values.
    status_t loadByteMetadata(std::ifstream *infile, const std::string &keyLine,
            CameraMetadata *metadata, uint32_t tag);

    // Load an array of floats and set metadata with the tag and values.
    status_t loadFloatMetadata(std::ifstream *infile, const std::string &keyLine,
            CameraMetadata *metadata, uint32_t tag, const char *delimiters = nullptr);

    // Load an array of doubles and set metadata with the tag and values.
    status_t loadDoubleMetadata(std::ifstream *infile, const std::string &keyLine,
            CameraMetadata *metadata, uint32_t tag, const char *delimiters = nullptr);

    // Load an array of rationals and set metadata with the tag and values.
    status_t loadRationalMetadata(std::ifstream *infile, const std::string &keyLine,
            CameraMetadata *metadata, uint32_t tag, const char *delimiters = nullptr);

    // Load an array of floats.
    status_t loadFloats(std::ifstream *infile, const std::string &keyLine, const char *delimiters,
            std::vector<float> *values);

    // Load lens shading map.
    status_t loadLensShadingMap(std::ifstream *infile, const std::string &keyLine,
            CameraMetadata *metadata);

    // Find all DNG filenames in mDir.
    void findAllDngFilenames();

    // Load a buffer from a DNG file.
    status_t loadRaw10BufferFromFile(void *buffer, size_t bufferSize, const std::string &filename);

    // Load a frame metatadata from a metadata file.
    status_t loadFrameMetadataFromFile(CameraMetadata *metadata, uint32_t frameNum,
            const std::string &filename);

    // Convert a raw16 buffer to compact raw10 buffer.
    status_t convertRaw16ToRaw10(void *raw10Dst, size_t raw10Size,
            const std::vector<uint16_t> &raw16Src, uint16_t whiteLevel);

    // Extract a vector of entries that are separated by characters specified in delimiters.
    status_t extractEntries(std::vector<std::string> *entries, std::string line,
        const char *delimiters = nullptr);

    // Directory where the input burst files and metadata files are.
    std::string mDir;

    // DNG filenames found in mDir.
    std::vector<std::string> mDngFilenames;
};

}// namespace android

#endif // PAINTBOX_HDR_PLUS_CLIENT_TEST_BURST_INPUT_H
