#ifndef GOOGLEX_GCAM_IMAGE_PROC_COLOR_SATURATION_H_
#define GOOGLEX_GCAM_IMAGE_PROC_COLOR_SATURATION_H_

#include <cassert>
#include <cmath>
#include <cstdint>

#include "googlex/gcam/image/t_image.h"

namespace gcam {

struct Context;

// Color saturation enhancement in Gcam should always be performed as follows:
//
//   1. Start with a (gamma-corrected) sRGB color (r, g, b), likely uint8_t or
//      uint16_t, and the maximum possible color value (white_level).
//
//   2. Take the min and max of the 3 channels.
//     float min_val = std::min(r, std::min(g, b));
//     float max_val = std::max(r, std::max(g, b));
//
//   3. Figure out how much to saturate this pixel, and the 'center' value
//      from which to push the r,g,b values.  (Be sure to use these
//      templatized helper functions, so that the logic for these decisions
//      remains centralized.)
//
//     float saturation = GetSaturationStrength<float, white_level>(
//         highlight_saturation, shadow_saturation, max_val);
//     float center = GetSaturationCenterValue<float>(min_val, max_val);
//
//   4. Push the (sRGB) color away from 'center' by 'saturation':
//     r = r + gcam::RoundToNearest((r - center) * saturation);
//     g = g + gcam::RoundToNearest((g - center) * saturation);
//     b = b + gcam::RoundToNearest((b - center) * saturation);

// LINT.IfChange
// GetSaturationStrength():
// During color saturation enhancement, for a given sRGB pixel, this helper
//   function determines what the saturation strength should be.
// Templatized so that it can be called from C or Halide.
// 'white_level' should contain the maximum possible value of 'lightness'.
//   (It is given as a template parameter to ensure that taking its reciprocal
//   is done at compile-time, to guarantee fast runtime performance.)
// 'lightness' should be set to (min(r,g,b) + max(r,g,b))/2 for the pixel.
// Recommended usage in C:      GetSaturationStrength<float, ##>
// Recommended usage in Halide: GetSaturationStrength<Halide::Expr, ##>
template <typename T, int white_level>
T GetSaturationStrength(T highlight_saturation_strength,  // From tuning.
                        T shadow_saturation_strength,     // From tuning.
                        T lightness) {
  // Linear interpolation here would leave the midtones too saturated, so this
  // formula instead biases the interpolation toward the
  // highlight_saturation_strength; 'shadow_saturation_strength' is used just
  // for the deeper shadows.
  return highlight_saturation_strength +
      ((white_level - lightness) * (1.0f / white_level)) *
      ((white_level - lightness) * (1.0f / white_level)) *
      (shadow_saturation_strength - highlight_saturation_strength);
}
// LINT.ThenChange(//depot/google3/googlex/gcam/ae/ae_finish_halide.cc,//depot/google3/googlex/gcam/finish_raw/finish_raw/finish_raw_halide.cc)

// GetSaturationCenterValue*():
// During color saturation enhancement, these helper functions determine, given
//   an sRGB color, the center (or pivot) value away from which the r,g,b values
//   will be pushed, in order to increase the color saturation.
// There are two versions - one for float, one for fixed-point.

// This is the float version.  It can be called from regular C or Halide code.
// 'T' should be either float or Halide::Float.
template <typename T>
T GetSaturationCenterValue(T min_val, T max_val) {
  return (min_val * 0.5f + max_val * 0.5f);
}

// This is the fixed-point version.  It can be called from regular C or Halide
//   code.
// The following types are recommended:
//   C usage:      GetSaturationCenterValueX2<uint16_t>
//   Halide usage: GetSaturationCenterValueX2<Halide::Expr>
template <typename T>
T GetSaturationCenterValueX2(T min_val, T max_val) {
  return min_val + max_val;
}

// Color saturation parameters.
struct ColorSatParams {
 public:
  // The amount by which to increase color saturation in (gamma-corrected) sRGB
  //   space, where color saturation is defined as the separation between the
  //   min and max color channel.
  // For example:
  //   A value of 0 will completely desaturate to grey;
  //   1.0 will have no effect;
  //   1.1 will increase the color separation (between min and max) by ~10%;
  //   2.0 will roughly double the separation;
  //   and so on.
  // The saturation amount can be tuned differently for shadows vs. highlights.
  //   The lightness of an sRGB-space pixel's (r,g,b) values are used to
  //   determine if it is a shadow or a highlight.  If this value is 0, it is a
  //   shadow; if this value is the highest possible value, it is a highlight;
  //   and in between these two values, the actual saturation amount to be used
  //   is interpolated (not necessarily linearly) between the two values here.
  //   For the interpolation function, see GetSaturationStrength().
  float highlight_saturation = 1.0f;
  float shadow_saturation = 1.0f;

  bool IsIdentity() const {
    return (fabsf(highlight_saturation - 1.0f) < 1.0f / 256.0f) &&
           (fabsf(shadow_saturation - 1.0f) < 1.0f / 256.0f);
  }
};

// Applies the given color saturation to an image.
bool ApplyColorSaturation(const InterleavedWriteViewU8& rgb,
                          const ColorSatParams& color_sat_params,
                          const Context& gcam_context);

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_PROC_COLOR_SATURATION_H_
