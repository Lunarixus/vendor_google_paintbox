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
//
// IMPORTANT: There is one significant exception: The raw finish pipeline
// doesn't actually use 'GetSaturationCenterValue'; it uses a different
// formula: the average of the RGB values.  This means AE's predictions are
// actually incorrect.  For more information, see the notes in
// finish_raw_halide.cc.
// TODO(geiss): Fix this.

// GetSaturationStrength():
// During color saturation enhancement, for a given sRGB pixel, this helper
//   function determines what the saturation strength should be.
// Templatized so that it can be called from C or Halide.
// 'white_level' should contain the maximum possible value of 'max_rgb'.
//   (It is given as a template parameter to ensure that taking its reciprocal
//   is done at compile-time, to guarantee fast runtime performance.)
// 'max_rgb' should be set to max(r,g,b) for the pixel.
// Recommended usage in C:      GetSaturationStrength<float, ##>
// Recommended usage in Halide: GetSaturationStrength<Halide::Expr, ##>
template <typename T, int white_level>
T GetSaturationStrength(T highlight_saturation_strength,  // From tuning.
                        T shadow_saturation_strength,     // From tuning.
                        T max_rgb) {
  // Linear interpolation here would leave the midtones too saturated, so this
  // formula instead biases the interpolation toward the
  // highlight_saturation_strength; 'shadow_saturation_strength' is used just
  // for the deeper shadows.
  return highlight_saturation_strength +
      ((white_level - max_rgb) * (1.0f / white_level)) *
      ((white_level - max_rgb) * (1.0f / white_level)) *
      (shadow_saturation_strength - highlight_saturation_strength);
}

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

class ColorSatSubParams {
 public:
  static const int kLutSize = (128 * (128 + 1) / 2);

  ColorSatSubParams();
  ~ColorSatSubParams();

  // Copy constructors.
  ColorSatSubParams(const ColorSatSubParams& src) {
    lut_ = nullptr;
    CopyFrom(src);
  }
  ColorSatSubParams& operator=(const ColorSatSubParams& src) {
    CopyFrom(src);
    return *this;
  }

  inline bool IsReady() const { return (lut_ != nullptr) || !UsesVibrance(); }

  // This call applies new settings for the saturation and vibrance.
  // Note that this call will be very fast iff sat_exp is 1.0f (~no vibrance),
  //   as the LUT generation can be skipped.
  void Update(float highlight_saturation,
              float shadow_saturation,
              float sat_exp);

  // Applies the current settings for saturation & vibrance to an image.
  void ProcessImage(const InterleavedWriteViewU8& rgb,
                    const Context& gcam_context) const;
  void ProcessImageReference(const InterleavedWriteViewU8& image) const;

  inline bool UsesSaturation() const {
    return (fabsf(highlight_saturation_ - 0) >= (1.0f / 256.0f)) ||
           (fabsf(shadow_saturation_ - 0) >= (1.0f / 256.0f));
  }
  inline bool UsesVibrance() const {
    return (fabsf(sat_exp_ - 1) >= (1.0f / 256.0f));
  }
  inline bool IsIdentity() const {
    return !(UsesSaturation() || UsesVibrance());
  }
  inline float GetHighlightSaturation() const { return highlight_saturation_; }
  inline float GetShadowSaturation() const { return shadow_saturation_; }
  inline float GetSatExp() const { return sat_exp_; }

  // You should only call this function if vibrance is being used (i.e.
  // UsesVibrance() returns true).
  inline int16_t ReadLut(int index) const {
    assert(UsesVibrance());
    assert(lut_);
    assert(index >= 0 && index < kLutSize);
    return lut_[index];
  }

 protected:
  void Clear();  // Frees the old LUT, if any.

  // The amount by which to increase color saturation in (gamma-corrected) sRGB
  //   space, where color saturation is defined as the separation between the
  //   min and max color channel.
  // For example:
  //   A value of -1.0 will completely desaturate to grey;
  //   0.0 will have no effect;
  //   0.1 will increase the color separation (between min and max) by ~10%;
  //   1.0 will roughly double the separation;
  //   and so on.
  // The saturation amount can be tuned differently for shadows vs. highlights.
  //   The maximum of an sRGB-space pixel's (r,g,b) values are used to determine
  //   if it is a shadow or a highlight.  If this value is 0, it is a shadow; if
  //   this value is the highest possible value, it is a highlight; and in
  //   between these two values, the actual saturation amount to be used
  //   is interpolated (not necessarily linearly) between the two values here.
  //   For the interpolation function, see lib_gcam/color_saturation.h.
  float highlight_saturation_;
  float shadow_saturation_;

  // Affects only low-saturation colors.
  // This is the exponent applied to saturation channel:
  //   sat' = pow(sat, sat_exp))
  //   where sat is in [0..1].
  // Possible values:
  //    1 = has no effect.
  //  < 1 = increase color saturation (in unsaturated colors only).
  //  > 1 = decrease color saturation (in unsaturated colors only).
  // Recommended: 0.75.
  float sat_exp_;

  // LUT that helps us use two values (min(r,g,b) and max(r,g,b)) to look
  // up a single precomputed value that will make saturation changes fast.
  // Only allocated and used when vibrance is needed.
  int16_t* lut_;

  // max_val ranges from [0..255] in steps of 2.
  // min_val ranges from [0..max_val] in steps of 2.
  // Therefore, instead of storing 128 * 128 values,
  //   we can do smart indexing and store half as many values;
  //   the table ends up being 8K entries, and at 2 bytes/entry,
  //   that's 16 KB -- small enough to fit in L1 cache.
  static inline int GetColorSatLutIndex(uint8_t min_val, uint8_t max_val) {
    return ((((max_val >> 1) * ((max_val >> 1) + 1)) >> 1) + (min_val >> 1));
  }

  void CopyFrom(const ColorSatSubParams& src);
};

// Per-device configurable tuning for color saturation in the final image.
struct ColorSatParams {
  ColorSatSubParams ldr;  // Params for color saturation in non-HDR scenes.
  ColorSatSubParams hdr;  // Params for color saturation in HDR scenes.
};

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_PROC_COLOR_SATURATION_H_
