#ifndef GOOGLEX_GCAM_IMAGE_IO_TMP_HELPER_H_
#define GOOGLEX_GCAM_IMAGE_IO_TMP_HELPER_H_

#include <string>

#include "googlex/gcam/image/t_image.h"

namespace gcam {

// Write an image to a base64 encoded TMP string (conforming with section 4 of
// RFC 4648). A TMP has a header:
//
//   struct {
//     int32_t width;
//     int32_t height;
//     int32_t frames;  // 1 from this function.
//     int32_t channels;
//     int32_t type;  // 2 for U8 images.
//   };
//
// Followed by the data, stored with dense strides, with dimensions ordered
// x, y, c innermost to outermost (smallest stride to largest stride).
std::string WriteImageToBase64Tmp(const PlanarReadViewU8& img);

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_IO_TMP_HELPER_H_
