#ifndef GOOGLEX_GCAM_IMAGE_METADATA_IMAGE_ROTATION_H_
#define GOOGLEX_GCAM_IMAGE_METADATA_IMAGE_ROTATION_H_

#include <cassert>
#include <string>

namespace gcam {

// Orientations, corresponding to rotation by a multiple of 90 degrees.
// The description is how the raw data should be transformed to display
// on-screen with correct orientation.  (Not to be confused with what EXIF
// reports, which is the inverse of this - effectively, it reports what you
// would have to do to the final image in order to go back to the original,
// sensor-orientation image.)
enum class ImageRotation {
  kCw = 0,
  k180,
  kCcw,
  kNone,
  kInvalid
};

std::string ToText(ImageRotation rotation);
ImageRotation TextToImageRotation(const std::string& text);

// Reverse the direction of an image rotation.
ImageRotation ReverseRotation(const ImageRotation& rotation);

void GetRotatedImageSize(const ImageRotation& rotation,
                         const int width,
                         const int height,
                         int* new_width,
                         int* new_height);

// Given pixel coordinates (sx, sy) in an image of resolution (width, height),
// compute the image coordinates they would map to if the image were rotated
// as indicated by 'rotation'.
inline void RotateCoordinates(const int sx, const int sy,
                              const int width, const int height,
                              const ImageRotation rotation,
                              int* dx, int* dy) {
  switch (rotation) {
    case ImageRotation::kCw:   // x -> y, y -> size-x
      *dx = height - 1 - sy;
      *dy = sx;
      break;
    case ImageRotation::kCcw:  // x -> size-y, y -> x
      *dx = sy;
      *dy = width - 1 - sx;
      break;
    case ImageRotation::k180:  // x -> size-x, y -> size-y
      *dx = width - 1 - sx;
      *dy = height - 1 - sy;
      break;
    case ImageRotation::kNone:
      *dx = sx;
      *dy = sy;
      break;
    default:
      assert(0);
  }
}

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_METADATA_IMAGE_ROTATION_H_
