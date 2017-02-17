#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_IMAGE_PROC_RESAMPLE_QUALITY_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_IMAGE_PROC_RESAMPLE_QUALITY_H_

namespace gcam {

enum class ResampleQuality {
  kLow = 0,  // Nearest-neighbor.
  kMedium,   // Bilinear (upsampling), integration/sampling (downsampling).
  kHigh,     // Unimplemented.
};

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_IMAGE_PROC_RESAMPLE_QUALITY_H_
