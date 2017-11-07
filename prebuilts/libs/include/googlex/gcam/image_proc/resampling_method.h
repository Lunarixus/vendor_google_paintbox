#ifndef GOOGLEX_GCAM_IMAGE_PROC_RESAMPLING_METHOD_H_
#define GOOGLEX_GCAM_IMAGE_PROC_RESAMPLING_METHOD_H_

#include <string>

namespace gcam {

enum class ResamplingMethod {
  kNone = 0,  // No resampling.
  kLanczos,   // Lanczos filter, possibly including mild sharpening.
  kRaisr,     // RAISR (see go/raisr).
  kInvalid,   // Invalid resampling method.
};

std::string ToText(ResamplingMethod resampling_method);
ResamplingMethod TextToResamplingMethod(const std::string& text);

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_PROC_RESAMPLING_METHOD_H_
