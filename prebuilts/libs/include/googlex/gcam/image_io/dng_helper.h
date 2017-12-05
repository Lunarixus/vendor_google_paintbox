#ifndef GOOGLEX_GCAM_IMAGE_IO_DNG_HELPER_H_
#define GOOGLEX_GCAM_IMAGE_IO_DNG_HELPER_H_

#include <cstdint>
#include <string>

#include "googlex/gcam/image/t_image.h"
#include "googlex/gcam/image_metadata/exif_metadata.h"

namespace gcam {

struct Context;
class RawImage;
class RawReadView;

// Get the dimensions of the final rendered DNG image, accounting for image
// rotation and the default cropping specified by the 'DefaultCropSize' tag.
void GetFinalDngImageSize(const InterleavedReadViewU16& image,
                          const ExifMetadata& exif_metadata,
                          int* width,
                          int* height);

// Write a raw image with metadata to a DNG file on disk.
// Returns whether it succeeded.
// If 'compress_output' is set to true, compresses the DNG contents using
// lossless Huffman JPG.
// TODO(nealw): Add XMP support to DNGs.
bool WriteDng(const std::string& filename, const InterleavedReadViewU16& image,
              const ExifMetadata& exif_metadata, bool compress_output = false,
              const Context* context = nullptr);
bool WriteDng(const std::string& filename, const RawReadView& image,
              const ExifMetadata& exif_metadata, bool compress_output = false,
              const Context* context = nullptr);

// Write a raw image with metadata to a DNG file in memory.
// Returns whether it succeeded.
// This function will allocate the memory and return the buffer, as well as
// its size in bytes. The caller must free the memory later using delete[].
bool WriteDngToMemory(uint8_t** out_buffer,
                      uint32_t* out_buffer_size_bytes,
                      const InterleavedReadViewU16& image,
                      const ExifMetadata& exif_metadata,
                      bool compress_output = false,
                      const Context* context = nullptr);
bool WriteDngToMemory(uint8_t** out_buffer,
                      uint32_t* out_buffer_size_bytes,
                      const RawReadView& image,
                      const ExifMetadata& exif_metadata,
                      bool compress_output = false,
                      const Context* context = nullptr);

// Reads a DNG file from disk into an RawImage. Populates the ExifMetadata
// struct with the resulting metadata.  Returns a null image on failure.
//
// DNG files in their full generality are not handled. For example, we hardcode
// assumptions about the number of Bayer channels and the number of color planes
// (4 and 3, respectively), and do not currently handle bit packing.
RawImage ReadDng(
    const std::string& filename, ExifMetadata* exif_metadata,
    TImageSampleAllocator* custom_allocator = TImageDefaultSampleAllocator(),
    const Context* context = nullptr);

// Reads a DNG file from memory into an RawImage. Populates the ExifMetadata
// struct with the resulting metadata.  Returns a null image on failure.
//
// DNG files in their full generality are not handled. For example, we hardcode
// assumptions about the number of Bayer channels and the number of color planes
// (4 and 3, respectively), and do not currently handle bit packing.
RawImage ReadDngFromMemory(
    uint8_t* buffer, uint32_t buffer_size_bytes, ExifMetadata* exif_metadata,
    TImageSampleAllocator* custom_allocator = TImageDefaultSampleAllocator(),
    const Context* context = nullptr);

// Reads metadata from an encoded DNG, respectively from a file on disk or
// memory buffer. The metadata is read without loading the image, so this is
// significantly faster than the ReadDng routines above.
//
// Returns false on failure.
bool ReadDngMetadata(const std::string& filename, ExifMetadata* exif_metadata);
bool ReadDngMetadataFromMemory(uint8_t* buffer, uint32_t buffer_size_bytes,
                               ExifMetadata* exif_metadata);

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_IO_DNG_HELPER_H_
