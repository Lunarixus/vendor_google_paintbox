#ifndef GOOGLEX_GCAM_IMAGE_PROC_ROW_ARTIFACTS_H_
#define GOOGLEX_GCAM_IMAGE_PROC_ROW_ARTIFACTS_H_

#include <array>
#include <vector>

#include "googlex/gcam/image_raw/raw.h"

namespace gcam {

class LogSaver;
struct Context;

// Describes a filter with a transfer function H(z) = Y(z)/X(z), where
// Y(z) = b0 + b1*z^-1 + b2*z^-2, and X(z) = 1 + a1*z^-1 + a2*z^-2
struct SecondOrderFilter {
  float b0, b1, b2;
  float a1, a2;
};

// Describes a periodic row artifact to be suppressed.
struct RowPattern {
  // Filter for passing the artifact. The filters are applied in sequence (so as
  // to construct a fourth order filter).
  std::array<SecondOrderFilter, 2> filter;

  // The gain of the filter at f = 1/period (measured as a fraction of the
  // sample rate).
  float gain_at_period;

  // The period of the artifact, in pixels.
  float period;

  // The expected peak amplitude of the artifact, in normalized pixel values
  // [0, 1]. If the amplitude is larger than this, the filter response is
  // ignored.
  float amplitude;
};

// Perform row noise and periodic row artifact suppression on a raw image. The
// var_noise vector defines a series of expected row noise local variances,
// variances[n] is the expected row noise variance with a radius of 2^n rows,
// measured in digital code values. The patterns vector is a list of RowPattern
// tuning objects, providing a filter and description for the periodic artifact.
// The processing is performed in-place on the frame.
void SuppressRowArtifacts(const std::vector<float>& var_noise,
                          const std::vector<RowPattern>& patterns,
                          int white_level, const Context& gcam_context,
                          LogSaver* log_saver, RawWriteView frame);

inline void SuppressRowArtifacts(const std::vector<float>& var_noise,
                                 int white_level, const Context& gcam_context,
                                 LogSaver* log_saver, RawWriteView frame) {
  SuppressRowArtifacts(var_noise, {}, white_level, gcam_context, log_saver,
                       frame);
}
inline void SuppressRowArtifacts(const std::vector<RowPattern>& patterns,
                                 int white_level, const Context& gcam_context,
                                 LogSaver* log_saver, RawWriteView frame) {
  SuppressRowArtifacts({}, patterns, white_level, gcam_context, log_saver,
                       frame);
}

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_PROC_ROW_ARTIFACTS_H_
