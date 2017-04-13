#ifndef GOOGLEX_GCAM_IMAGE_ICC_PROFILE_H_
#define GOOGLEX_GCAM_IMAGE_ICC_PROFILE_H_

#include <cstddef>
#include <cstdint>

namespace gcam {

// Supported ICC profiles:
//
// IccProfile::kSrgb
//   [sRGB_parametric.icc]
//   sRGB v4 profile from hzeng@ (2017), with a parametric tone curve and
//   without black scaling.
//
// IccProfile::kP3
//   [Display_P3_parametric.icc]
//   Display P3 v4 profile from hzeng@ (2017), with a parametic tone curve and
//   without black scaling. This is effectively the same as DCI-P3: it uses the
//   same primaries, D65 whitepoint, and the sRGB tone curve.
//
// *** NOTE ***
//   All of these color spaces use the same tone curve. If we add a new color
//   space with a different curve, Gcam's tonemapping needs to be updated to
//   take this into account.

enum class IccProfile {
  kNone = 0,
  kSrgb,
  kP3,
  kInvalid,
};

const char* ToText(IccProfile icc_profile);
IccProfile TextToIccProfile(const char* text);

// Look up an embedded binary ICC profile, based on the enum.
// NOTE: Unimplemented on Apple platforms (always returns false).
bool GetIccProfileData(IccProfile icc_profile, const uint8_t** icc_data,
                       size_t* icc_size);

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_ICC_PROFILE_H_
