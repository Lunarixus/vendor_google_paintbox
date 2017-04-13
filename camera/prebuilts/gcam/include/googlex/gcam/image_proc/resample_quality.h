#ifndef GOOGLEX_GCAM_IMAGE_PROC_RESAMPLE_QUALITY_H_
#define GOOGLEX_GCAM_IMAGE_PROC_RESAMPLE_QUALITY_H_

namespace gcam {

enum class ResampleQuality {
  kLow = 0,  // Nearest-neighbor.
  kMedium,   // Bilinear (upsampling), integration/sampling (downsampling).
  kHigh,     // Unimplemented.
};

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_PROC_RESAMPLE_QUALITY_H_
