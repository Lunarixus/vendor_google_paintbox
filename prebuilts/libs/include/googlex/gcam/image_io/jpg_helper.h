#ifndef GOOGLEX_GCAM_IMAGE_IO_JPG_HELPER_H_
#define GOOGLEX_GCAM_IMAGE_IO_JPG_HELPER_H_

#include <cstdint>
#include <string>

#include "googlex/gcam/image/t_image.h"
#include "googlex/gcam/image/yuv.h"

namespace gcam {

struct ExifMetadata;

// ENCODING FUNCTIONS:

// The quality value is expressed on the 0..100 scale recommended by IJG.
const int kMinJpgQuality = 0;
const int kMaxJpgQuality = 100;

// By default, match the JPG quality level used for non-Gcam images and
// and thumbnails on Glass.
extern const int kDefaultJpgQuality;
const int kDefaultJpgQualityThumbnail = 60;

// The MakerNote size is limited by the maximum size of the EXIF APP1 segment.
const int kMaxMakernoteSize = 65535;

// Encodes image data to a JPG file, with support for grayscale (1 channel),
// RGB (3 channels), or YUV (semiplanar, NV21 or NV12).
//
// By default, no EXIF data, XMP data or ICC profile will be written, unless the
// optional 'exif_data' field is specified.
bool WriteJpg(const std::string& filename, const InterleavedReadViewU8& image,
              int quality = kDefaultJpgQuality,
              const ExifMetadata* exif_data = nullptr);
bool WriteJpg(const std::string& filename, const YuvReadView& image,
              int quality, const ExifMetadata* exif_data);

// Encodes image data to a JPG file in memory, with support for grayscale
// (1 channel), RGB (3 channels), or YUV (semiplanar, NV21 or NV12).
//
// These functions allocate the memory and return the buffer, as well as its
// size in bytes. The caller must free the memory later using
// "free(out_buffer)".
//
// By default, no EXIF data, XMP data or ICC profile will be written, unless the
// optional 'exif_data' field is specified.
bool WriteJpgToMemory(uint8_t** out_buffer, uint32_t* out_buffer_size_bytes,
                      const InterleavedReadViewU8& image,
                      int quality = kDefaultJpgQuality,
                      const ExifMetadata* exif_data = nullptr);
bool WriteJpgToMemory(uint8_t** out_buffer, uint32_t* out_buffer_size_bytes,
                      const YuvReadView& image,
                      int quality = kDefaultJpgQuality,
                      const ExifMetadata* exif_data = nullptr);

// DECODING FUNCTIONS:

// Decodes a JPG image on disk.
// Optionally decode EXIF and XMP metadata into ExifMetadata, if EXIF or XMP is
//   found and a desination for this is provided by the caller.
// NOTE: EXIF decoding is *extremely* limited. Only a few tags are parsed.
// NOTE: XMP data is only read in as a binary blob.
// Returns a null image on failure.
InterleavedImageU8 DecodeJpgFromDisk(const std::string& filename,
                                     ExifMetadata* exif_metadata = nullptr);

// Decodes a compressed JPG image in memory, to an uncompressed buffer
//   in memory.
// Optionally decode EXIF and XMP metadata into ExifMetadata, if EXIF or XMP is
//   found and a desination for this is provided by the caller.
// NOTE: EXIF decoding is *extremely* limited. Only a few tags are parsed.
// NOTE: XMP data is only read in as a binary blob.
// Returns a null image on failure.
InterleavedImageU8 DecodeJpgFromMemory(const uint8_t* jpeg_blob_in_memory,
                                       const uint32_t jpeg_blob_byte_count,
                                       ExifMetadata* exif_metadata = nullptr);

// MAKERNOTE FUNCTIONS:

// Encodes a string for the purpose of writing it in the EXIF MakerNote field,
//   using weak encryption to obfuscate its contents.
// For the variant using a preallocated char[] buffer, the client must make
//   sure that the buffer is at least 'kMaxMakernoteSize' bytes.
std::string EncodeMakerNote(const std::string& str);
void EncodeMakerNote(const char* str, char out_buffer[], int* out_buffer_size);

// Decode an EXIF MakerNote field previously written by EncodeMakerNote(),
// and return whether decoding was successful.
bool DecodeMakerNote(const std::string& str, std::string* result);

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_IO_JPG_HELPER_H_
