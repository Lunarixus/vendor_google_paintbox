#ifndef GOOGLEX_GCAM_IMAGE_PROC_RESAMPLING_METHOD_H_
#define GOOGLEX_GCAM_IMAGE_PROC_RESAMPLING_METHOD_H_

namespace gcam {

enum class ResamplingMethod {
  kNone = 0,  // No resampling.
  kLanczos,   // Lanczos filter, possibly including mild sharpening.
  kRaisr,     // RAISR (see go/raisr).
  kInvalid,   // Invalid resampling method.
};

const char* ToText(ResamplingMethod resampling_method);
ResamplingMethod TextToResamplingMethod(const char* text);

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_PROC_RESAMPLING_METHOD_H_
