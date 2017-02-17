#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_TONEMAP_TONEMAP_YUV_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_TONEMAP_TONEMAP_YUV_H_

// Tonemapping curves for the legacy YUV pipeline.

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace gcam {

// Control point for a floating point tonemapping curve.
struct TonemapFloatControlPoint {
  float key;
  float value;
};

// Floating point tonemapping curve, specified by a set of control points
// in [0,1] x [0,1], with linear interpolation defined in between them.
// The interpolation of this curve as piecewise linear is consistent with the
// linear interpolation mandated by the Android camera HAL.
//
// The input keys must be distinct and increasing, its keys must cover the full
// input range, [0,1], and its values must be monotonically increasing. The
// last two requirements are specific to Gcam, which is stricter than the
// Android camera HAL about valid input.
struct TonemapFloat {
  TonemapFloat() { Clear(); }
  void Clear() { control_points.clear(); }
  bool Check() const;

  std::vector<TonemapFloatControlPoint> control_points;
};

static const uint16_t kTonemapMaxValue    = 255;
static const uint16_t kRevTonemapMaxValue = 1023;

// Lookup table mapping linear 10-bit input to tonemapped 8-bit output.
// Output values are in the range [0..kTonemapMaxValue].
struct Tonemap {
  Tonemap() { MakeInvalid(); }
  void MakeInvalid() { values[1023] = 0; }  // Lightweight "clear".
  void Clear() { memset(this, 0, sizeof(Tonemap)); }
  bool Check() const;
  void SerializeToString(std::string* str) const;
  bool DeserializeFromString(const char** str_ptr);
  bool Equals(const Tonemap& other) const;

  uint8_t values[1024];
};

// Lookup table mapping tonemapped 8-bit input BACK to linear 10-bit output.
// Output values are in the range [0..kRevTonemapMaxValue].
struct RevTonemap {
  RevTonemap() { MakeInvalid(); }
  void MakeInvalid() { values[255] = 0; }  // Lightweight "clear".
  void Clear() { memset(this, 0, sizeof(RevTonemap)); }
  bool Check() const;

  uint16_t values[256];
};

// Given a tonemapping curve specified as a mapping from 10-bit values (linear)
// to 8-bit values (tonemapped), generate an inverse curve mapping from 8 bits
// (tonemapped) to 10 bits (linear).
RevTonemap ReverseTonemapCurve(const Tonemap& curve);

// General functions to help you smooth out tonemapping curves.
void SmoothValuesU8(uint8_t* curve, const int num_values,
                    const int max_output_val, const int rad,
                    const float output_scale, const bool verbose);
void SmoothValuesU16(uint16_t* curve, const int num_values,
                     const int max_output_val, const int rad,
                     const float output_scale, const bool verbose);

// Normal tonemapping using Tonemap is 10 -> 8 bits; this makes the output also
// be 10 bits. The resulting curve is smoothed a bit to mitigate quantization.
void TenBitTonemap(const Tonemap& tonemap, uint16_t tonemap10[1024]);

// Normal reverse-tonemapping using RevTonemap is 8 -> 10 bits; this makes the
// input be 10 bits too. The resulting curve is smoothed a bit to mitigate
// quantization.
void TenBitRevTonemap(const RevTonemap& rev_tonemap,
                      uint16_t rev_tonemap10[1024]);

//////////////////////////////////////////////////////////////////////////////
// Specific tonemapping curves for the legacy YUV pipeline.

// The output tonemap curve used by the YUV pipeline.  (Not to be confused with
// the YUV pipeline's *capture* tonemap curve, which is different.)
const Tonemap& YuvPipelineOutputGammaPlusTonecurve();

// Basic Gcam input tonemapping.  Linear at first, then roughly gamma-1/2.2, for
// more precision where noise is low (in absolute terms). If you need to capture
// tonemapped YUV input, this curve should be used.
Tonemap GenGcamTonemap();

// Gcam input tonemapping for the Nexus 5. Similar to basic Gcam tonemapping,
// but derived from the constraint that the LUT in Qualcomm's ISP has only 64
// entries, and that it rounds down after interpolating the output.  Linear
// at first, then roughly gamma-1/2.2.
TonemapFloat GenGcamNexus5TonemapFloat();

// This is the initial version that was [inadvertently] used on Glass;
// it had way too much contrast.
Tonemap GenGlassIncorrectTonemap1();

// Galaxy Nexus stock camera app tonemapping.
//
// Note: For higher precision, a curve very similar to the one returned by
// GenStockGalaxyNexusTonemap() can be obtained by feeding the output of
// SmoothRec709Gamma() into the GcamTonecurve() function.
Tonemap GenStockGalaxyNexusTonemap();

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_TONEMAP_TONEMAP_YUV_H_
