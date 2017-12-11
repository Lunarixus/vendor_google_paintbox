#ifndef GOOGLEX_GCAM_IMAGE_IO_EXIF_ENCODE_H_
#define GOOGLEX_GCAM_IMAGE_IO_EXIF_ENCODE_H_

#include <cstdint>

#include "googlex/gcam/image_metadata/exif_metadata.h"

namespace gcam {

// Encodes Gcam EXIF metadata to a JPEG APP1 segment in memory.  This function
// will allocate the memory and return the buffer, as well as its size.  On
// failure, the function returns false and (*out_buffer) will be set to nullptr.
// Otherwise, the caller is responsible for freeing (*out_buffer) using
// delete[].
//
// The EXIF tags in 'kFixedTagTable' (see exif.h) will be included
// automatically, and if a thumbnail was provided, the EXIF tags in
// 'kFixedThumbnailTagTable' (likewise, see exif.h) will be included as well.
//
// NOTE: the encoded EXIF buffer does not include a JPEG APP1 marker, and its
// first two bytes (interpreted as big endian) describes the EXIF segment size.
// Some consumers, notably libjpeg, do not expect the two-byte segment size to
// be prepended to the EXIF segment.
bool EncodeGcamExif(int image_width,
                    int image_height,
                    const ExifMetadata& exif_data,
                    uint8_t** out_buffer,              // Output.
                    uint32_t* out_buffer_size_bytes);  // Output.

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_IO_EXIF_ENCODE_H_
