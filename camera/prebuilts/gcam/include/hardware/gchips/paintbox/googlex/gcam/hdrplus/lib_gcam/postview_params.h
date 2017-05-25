#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_POSTVIEW_PARAMS_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_POSTVIEW_PARAMS_H_

#include "googlex/gcam/image/pixel_format.h"

namespace gcam {

// Settings for how Gcam should generate the postview image.
//
// 'pixel_format' selects the postview image format.
//
// The size of the returned postview image is determined by 'target_width'
// and 'target_height':
//   - If you specify just one (and set the other to zero): the contents (~crop
//     area) of the postview, and its aspect ratio, will match the final image.
//   - If you specify both, but the aspect ratio of the requested postview does
//     not exactly match that of the final image, the postview contents will
//     show a vertical or horizontal crop (relative to the final image).
//   - If you specify neither (both are zero), then the postview will have its
//     longer edge set to kDefaultPostviewLongestEdge and have the same aspect
//     ratio as that of the input.
//
// The postview is unrotated, i.e., it matches the orientation of the payload
// images used to generate it.
struct PostviewParams {
  GcamPixelFormat pixel_format = GcamPixelFormat::kUnknown;
  int target_width = 0;
  int target_height = 0;
};

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_POSTVIEW_PARAMS_H_
