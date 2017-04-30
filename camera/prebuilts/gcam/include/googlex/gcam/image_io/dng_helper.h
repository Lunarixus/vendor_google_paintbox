#ifndef GOOGLEX_GCAM_IMAGE_IO_DNG_HELPER_H_
#define GOOGLEX_GCAM_IMAGE_IO_DNG_HELPER_H_

#include <cstdint>

#include "googlex/gcam/image/t_image.h"
#include "googlex/gcam/image_metadata/exif_metadata.h"

namespace gcam {

class Context;
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
bool WriteDng(const char* filename,
              const InterleavedReadViewU16& image,
              const ExifMetadata& exif_metadata,
              bool compress_output = false,
              const Context* context = nullptr);
bool WriteDng(const char* filename,
              const RawReadView& image,
              const ExifMetadata& exif_metadata,
              bool compress_output = false,
              const Context* context = nullptr);

// Write a raw image with metadata to a DNG file in memory.
// Returns whether it succeeded.
// This function will allocate the memory and return the buffer, as well as
// its dimensions. The caller must free the memory later using delete[].
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
RawImage ReadDng(const char* filename,
                 ExifMetadata* exif_metadata,
                 TImageSampleAllocator* custom_allocator =
                     TImageDefaultSampleAllocator(),
                 const Context* context = nullptr);

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_IO_DNG_HELPER_H_
