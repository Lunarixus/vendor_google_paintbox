#ifndef GOOGLEX_GCAM_AE_AE_SHOT_PARAMS_H_
#define GOOGLEX_GCAM_AE_AE_SHOT_PARAMS_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <string>
#include <vector>

#include "googlex/gcam/base/pixel_rect.h"

namespace gcam {

enum class HdrMode {
  kAuto = 0,
  kDisabled,
  kInvalid,
};

std::string ToText(HdrMode mode);
HdrMode TextToHdrMode(const std::string& text);

// This struct bundles together the minimal set of parameters needed to perform
//   AE (auto-exposure) on a single frame.
// In practice, when Gcam is running background AE on viewfinder frames, you
//   have to provide one of these structs for each viewfinder frame, so that
//   Gcam can process it correctly for AE.
// You also need to provide one of these on a real shot, where an AeShotParams
//   subset is nested inside the larger ShotParams, which contains all of the
//   information needed to process an entire shot.
struct AeShotParams {
  bool Equals(const AeShotParams& other) const;

  // The size of the payload frames that you will pass into Gcam, when you
  //   pass them in.
  // If you are manually handling digital zoom (which is generally NOT
  //   recommended) - by cropping frames, adjusting face locations and sizes,
  //   adjusting weighted metering areas, etc. - then the size here should
  //   reflect that crop, since it is done before the frame is passed to Gcam.
  // Requirements:
  //   1. These should be less than or equal to the
  //      payload_frame_raw_max_width/height values in the Tuning structure.
  //   2. These should exactly match the actual width & height of the
  //      *Bayer raw* payload frames you pass in for this shot.
  // If requirements 1 or 2 above are violated, then (respectively):
  //   1. Memory estimation will be incorrect, and warnings will be issued.
  //   2. Metering quality might be compromised, and warnings will be issued.
  // TODO(geiss): It would be nice to move these two params out of here and back
  //   into ShotParams.  Do a study to see if we can get away with ignoring,
  //   for AE purposes, the margin we'd chop off, in the real shot; if so, we
  //   could move these 2 params, and AE could just use 'crop' without having
  //   to use the more complex 'CropSchedule'.
  // In both cases, the aspect ratio and its inverse formed by these two values
  //   have to be less than kMaxOutputAspectRatio.
  // The default value of 0 forces the caller to set these accurately.
  int payload_frame_orig_width = 0;
  int payload_frame_orig_height = 0;

  // Desired cropping to apply to the shot, i.e. digital zoom or change in
  //   aspect ratio, specified with a normalized rectangle. Gcam will come as
  //   close as possible to realizing this exact crop window.
  // Note that the crop is relative to the full payload frames *after* cropping
  //   black pixels outside the active area, but before any other cropping or
  //   rotation.
  // TODO(geiss): There is currently a bug when using digital zoom with Smart
  //   Metering driven by *YUV* viewfinder frames. In this case, the HAL crops
  //   the actual YUV viewfinder frames (per spec), but Gcam rigidly expects all
  //   input frames to be uncropped, even during digital zoom, so we end up
  //   erroneously cropping the image *again* (for metering purposes).
  //   See b/20054665 for more details.
  // The aspect ratio and its inverse formed by this crop must be less than
  //   kMaxOutputAspectRatio.
  // Default: full frame (no digital zoom).
  NormalizedRect crop;

  // Allows manual influence over the spatial weighting used in Gcam's AE
  //   (auto-exposure), so that camera apps can implement features such as
  //   touch-to-expose.
  // Optional; to bypass this feature, simply leave the vector empty.
  // To use it, populate the vector with the regions of interest.  The
  //   coordinate system is designed to match the active pixel area of the
  //   *original* image given to Gcam, before any Gcam-side digital zoom (crop)
  //   is applied; i.e. the [0..1] x [0..1] range here must map to the full
  //   active area of the original frames you pass in to Gcam (...not to the
  //   digital zoom crop region).
  // The 'weight' for each region must be > 0.
  // If you specify any regions, then pixels NOT falling in a region
  //   will assume a weight of ZERO.
  // If a pixel falls into multiple specified regions, the weights for
  //   the various regions will be added at that pixel.
  // Note that Gcam's built-in AE already applies the following spatial
  //   adjustments, going into AE:
  //   1. Some CWA (center-weighted averaging).
  //   2. Faces are significantly boosted, in importance, for AE.
  // However, logically, if a pixel does not fall into at least one specified
  //   region, then even if it has a face, for example, its net weight (for
  //   metering purposes) would fall to ZERO.
  // If you would like to avoid this, and instead downweight (rather than
  //   completely ignore) the pixels not covered by your regions, simply
  //   add an extra region (rectangle) that covers the full [0..1] x [0..1]
  //   area.  This way, all pixels will count to some degree, but you can
  //   add additional regions to make certain pixels count even more.
  std::vector<WeightedNormalizedRect> weighted_metering_areas;

  // Exposure compensation, above or below auto-exposure, measured in stops.
  // Applies to the payload burst only.
  //    0 = no bias;
  //   +1 = capture twice as much light as normal;
  //   -1 = capture half as much light as normal;
  //   etc.
  float exposure_compensation = 0.0f;

  // Set to 'kAuto' (recommended) or 'kDisabled'.
  HdrMode hdr_mode = HdrMode::kAuto;

  // The target dimensions for the final output image, achieved through some
  //   combination of resampling and, if necessary to meet the target aspect
  //   ratio, cropping.
  // The exact target dimensions may not be realized exactly, due to internal
  //   constraints such as padding required by the image processing, even
  //   dimensions requirements, etc.
  // Note that the target width/height is specified in sensor orientation,
  //   before any rotation happens; the final output image may actually be a
  //   rotation of the adjusted target dimensions.
  // The final output will be *smaller or equal* to the target dimensions, in
  //   both width and height, but will approximately maintain the target aspect
  //   ratio. This is achieved by cropping along one dimension of the final
  //   output so the aspect ratio will match the target aspect ratio, up to
  //   rounding to even for aesthetics.
  // The target dimensions must both be positive and non-zero, and both the
  //   target aspect ratio and its inverse must also be less than
  //   kMaxOutputAspectRatio. Otherwise, processing will be aborted with an
  //   error logged.
  // TODO(hasinoff): Enforce that the aspect ratio of the target dimensions
  //   matches that of 'crop', up to some slop.
  // Default: 0 (invalid).
  int target_width = 0;
  int target_height = 0;
};

}  // namespace gcam

#endif  // GOOGLEX_GCAM_AE_AE_SHOT_PARAMS_H_
