#ifndef GOOGLEX_GCAM_IMAGE_PROC_GEOMETRIC_CORRECTION_H_
#define GOOGLEX_GCAM_IMAGE_PROC_GEOMETRIC_CORRECTION_H_

#include <string>

namespace gcam {

// Supported geometric correction:
//
// GeometricCorrection::kNone
//   No geometric correction applied.
//
// GeometricCorrection::kUndistortFaces
//   Correct perspective distortion on face regions. The effects are more
//   noticeable for wide-angle lenses and at the corners of the image.

enum class GeometricCorrection {
  kNone = 0,
  kUndistortFaces,
  kInvalid,
};

std::string ToText(GeometricCorrection geometric_correction);
GeometricCorrection TextToGeometricCorrection(const std::string& text);

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_PROC_GEOMETRIC_CORRECTION_H_
