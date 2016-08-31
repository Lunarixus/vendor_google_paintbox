#ifndef GOOGLEX_GCAM_FINISH_YUV_SHARPENER_H_
#define GOOGLEX_GCAM_FINISH_YUV_SHARPENER_H_

#include "googlex/gcam/image/t_image.h"

namespace gcam {

struct Context;

// Compute a 3x3 census transform of the given image, where the baseline
// value for each pixel is the pixel's value plus a caller-provided offset,
// baseline_offset.
//
// The census result is encoded in the 8 bits of the result image such that
// the low bit (bit 0) indicates whether the pixel offset (+1,+1) is above
// the baseline value, bit 1 indicates the result for the pixel (0, +1),
// 2 -> (-1, +1), 3 -> (+1, 0), 4 -> (-1, 0), 5 -> (+1, -1), 6 -> (0, -1),
// and 7 -> (-1, -1).
//
// For pixels along a single-pixel boundary, census values are copied from
// the nearest valid neighbor, one pixel toward the interior.
InterleavedImageU8* ComputeCensus(const InterleavedReadViewU8& image,
                                  int baseline_offset,
                                  void* user_context = nullptr);

// Reference implementation of the census transform in pure C++.
InterleavedImageU8* ComputeCensusReference(const InterleavedReadViewU8& image,
                                           int baseline_offset);

// Per-device configurable tuning settings for sharpening.
struct SharpenParams {
  float max_variance_mf;    // No mf sharpening above this noise level.
  float max_variance_hf;    // No hf sharpening above this noise level.
  float stddev_adjustment;  // Sharpening per-device adjustment on noise level.
  float mf_strength;        // Strength of medium-frequency sharpening.
  float hf_strength;        // Strength of high-frequency sharpening.

  // Enable the despeckle filter when the average noise standard deviation in
  // the luma channel exceeds this threshold.
  float min_std_for_despeckle;
};

// In-place, single-channel sharpening. For noisy/low-light images, a despeckle
// filter will also be applied after sharpening.
void Sharpen(const PlanarWriteViewU8* luma, float average_variance,
             float analog_gain, const SharpenParams& sharpen_params,
             const Context& gcam_context);

}  // namespace gcam

#endif  // GOOGLEX_GCAM_FINISH_YUV_SHARPENER_H_
