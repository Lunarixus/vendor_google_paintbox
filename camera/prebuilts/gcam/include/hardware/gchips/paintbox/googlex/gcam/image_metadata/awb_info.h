#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_IMAGE_METADATA_AWB_INFO_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_IMAGE_METADATA_AWB_INFO_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <string>
#include <utility>
#include <vector>

#include "hardware/gchips/paintbox/googlex/gcam/base/log_level.h"

namespace gcam {

enum class WhiteBalanceMode {
  kAuto = 0,
  kManual,
  kUnknown,
};

const char* ToText(WhiteBalanceMode mode);
WhiteBalanceMode TextToWhiteBalanceMode(const char* text);

const int kColorTempUnknown = 0;
const int kMinValidColorTemp = 300;
const int kMaxValidColorTemp = 9600;

struct AwbInfo {
  AwbInfo() { Clear(); }
  void Clear();
  bool Check() const;
  void Print(LogLevel log_level, int indent_spaces) const;
  void SerializeToString(std::string* str, int indent_spaces) const;
  bool DeserializeFromString(const char** str);
  bool Equals(const AwbInfo& other) const;
  void SetIdentityRgbToRgb();

  // Helper function to fetch the WB gains in simple R,G,B order.
  // G is formed by averaging Gr and Gb.
  void GetWbGainsRGB(float* r, float* g, float* b) const;

  // The color temperature of the scene's light source, in Kelvin.
  // OPTIONAL on devices where the ISP doesn't use color_temp (and instead
  //   only derives the AWB using the 4 gains).
  // On these systems, if unknown, set to kColorTempUnknown.
  // Values are usually in the ~3000...6000 range, but can go outside that.
  // In general, on a system where the ISP uses a concept
  //   of color temperature (rather than just the 4 gains), color_temp
  //   should be used everywhere (never kColorTempUnknown); on a system
  //   where the ISP doesn't use color temperature, the color_temp value
  //   will be mixed, as follows:
  //
  //     (color_temp is always...)          ISP uses color temp?
  //                                      Yes                 No
  //     Metering frame request         (valid)             (valid)
  //     Metering frame result          (valid)            (unknown)
  //     Payload frame request          (valid)            (unknown)
  //     Payload frame result           (valid)            (unknown)
  //
  int color_temp;

  // Gains for the 4 color channels, applied in a linear (pre-gamma/tonemap)
  //   color space.
  // Channel order here is: [R, Gr, Gb, B].
  float gains[4];

  // The 3x3 color conversion matrix (CCM).
  // The values are stored row-major, so indices: (0,1,2) = first row; etc.
  float rgb2rgb[9];
};

// 'TetToAwb' can be used to define an arbitrary map from a TET to an AwbInfo
// that is appropriate for it, for a given scene.  These are usually generated
// from a metering burst, but also work fine with just one metering frame (i.e.
// Smart Metering).
typedef std::pair<float, AwbInfo> TetAwbPair;
typedef std::vector<TetAwbPair> TetToAwb;

// Given a map from TET to AWB, constructed from the TET captured in the
// metering burst, estimate the AWB corresponding to the given TET.
AwbInfo GetAwbForTet(float final_tet, const TetToAwb& map, bool verbose);

// Interpolates between all values of the two structures, guided by 't' in the
// [0..1] range.  t==0 returns k1, t==1 returns k2, and values in-between
// are linearly interpolated (piecewise).
AwbInfo InterpolateWb(const AwbInfo& k1, const AwbInfo& k2, const float t);

// Old WB gains were stored in fixed point. If we parsed some old WB gains, it
// should be obvious, this function detects and fixes them.
void FixOldWbGains(AwbInfo* old_awb);

// Generate random white balance info.
AwbInfo GetRandomWhiteBalance();

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_IMAGE_METADATA_AWB_INFO_H_
