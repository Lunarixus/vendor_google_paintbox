#ifndef GOOGLEX_GCAM_IMAGE_METADATA_SPATIAL_GAIN_MAP_H_
#define GOOGLEX_GCAM_IMAGE_METADATA_SPATIAL_GAIN_MAP_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "googlex/gcam/base/pixel_rect.h"
#include "googlex/gcam/image/t_image.h"

namespace gcam {

// This map describes the gain that the ISP applied to the { R, Gr, Gb, B }
//   channels while they were still linear (*before tonemapping*).
// The values should be ~1.0 at the center, and > 1.0 as you go out
//   (usually 2-5, depending on how severe your camera module's
//   vignetting is, and how aggressively the ISP tried to correct it).
// The gain encoded in this table should be for JUST lens shading correction;
//   white balance gains should not be factored into it.
// The map has 4 channels, corresponding to the 4 Bayer colors
//   { R, Gr, Gb, B }, in that canonical order.
// A good recommended size for this map is around 40 x 30 -- big enough
//   to capture the lens shading curve, but not much bigger.
// This gain map should match the area on the sensor that the corresponding
//   image represents.  So, if the images passed to Gcam are cropped
//   in any way (...say you are doing 16:9 cropping at the sensor),
//   then you'll want to similarly crop this map, before passing it into
//   Gcam.
// Data interpretation:
//   Ideally, the gains along the edges of this map should represent the gains
//   applied to the very edge pixels of the hi-res images, and the gains for
//   all interior pixels in the hi-res image can be determined using bilinear
//   (or better) interpolation of the values in the SpatialGainMap.  This is
//   the interpretation used by our raw pipeline.  The YUV pipeline uses a
//   slightly different interpretation (see the comments in the ResizeAndCrop()
//   function for details), which is incorrect, but which we likely won't change
//   because it would involve a costly retuning of other parameters.
// Data ordering:
//   The values are stored interleaved (RGGB, RGGB...) and in row-major,
//   reading order.  That means that value_[0..3] are the RGGB gains for the
//   very upper-left-most pixel of the image, and the next 4 values are the
//   RGGB gains at some point directly to the right, and so on.
// Orientation:
//   SpatialGainMaps should correspond to the original sensor data.  Do not
//   rotate the data based on the camera orientation.
class SpatialGainMap {
 public:
  static const int kNumCh = 4;  // RGGB.

  SpatialGainMap(int w, int h, bool is_precise = false,
                 bool has_extra_vignetting_applied = false);
  explicit SpatialGainMap(const InterleavedReadViewF& gain_map);
  SpatialGainMap(const SpatialGainMap& src);
  SpatialGainMap() = default;
  SpatialGainMap(SpatialGainMap&& other) = default;

  SpatialGainMap& operator=(SpatialGainMap&& other) = default;
  SpatialGainMap& operator=(const SpatialGainMap& other);
  inline bool operator==(std::nullptr_t) const {
    return gain_map_ == nullptr;
  }
  inline bool operator!=(std::nullptr_t) const {
    return gain_map_ != nullptr;
  }
  inline SpatialGainMap& operator=(std::nullptr_t) {
    gain_map_ = nullptr;
    return *this;
  }
  inline operator bool() const { return *this != nullptr; }

  // Write or read the Spatial Gain Map natively, as a 4-channel map (RGGB).
  inline void WriteRggb(int x, int y, int ch, float value) {
    assert(x >= 0 && x < width() && y >= 0 && y < height() && ch >= 0 &&
           ch < num_channels());
    gain_map_.at(x, y, ch) = value;
  }
  inline float ReadRggb(int x, int y, int ch) const {
    assert(x >= 0 && x < width() && y >= 0 && y < height() && ch >= 0 &&
           ch < num_channels());
    return gain_map_.at(x, y, ch);
  }

  // Read the Spatial Gain Map as if it were a simple 3-channel map, where
  // the channel index [0,1,2] corresponds to R, G, B.
  inline float ReadRgb(int x, int y, int ch) const {
    assert(x >= 0 && x < width() && y >= 0 && y < height() && ch >= 0 &&
           ch < 3);
    switch (ch) {
      case 0:
        // Read the red channel.
        return gain_map_.at(x, y, 0);
      case 1:
        // Average the two green channels.
        return (gain_map_.at(x, y, 1) + gain_map_.at(x, y, 2)) * 0.5f;
      case 2:
        // Read the blue channel.
        return gain_map_.at(x, y, 3);
      default:
        assert(0);
        return 0;
    }
  }

  // Samples the spatial gain at a particular point in the map, using
  //   bilinear interpolation.  Uses the canonical channel order for the 4
  //   channels (RGGB).
  // Input coordinates are in [0..1] x [0..1] (stretched to fit any shape
  //   map), where (0,0) will exactly map to the upper-left value in the map,
  //   (1,1) will exactly map to the lower-right value in the map, and all
  //   and all other points in-between will be interpolations inside the map.
  inline float InterpolatedReadRggb(float fx, float fy, int ch) const {
    assert(fx >= 0);
    assert(fy >= 0);
    fx *= (width() - 1);
    fy *= (height() - 1);
    int ix = static_cast<int>(fx);
    int iy = static_cast<int>(fy);
    int ix2 = ix + 1;
    int iy2 = iy + 1;
    // Don't read off the right or bottom edges.
    if (ix2 > width() - 1) {
      ix2 = width() - 1;
      if (ix > width() - 1) {
        ix = width() - 1;
      }
    }
    if (iy2 > height() - 1) {
      iy2 = height() - 1;
      if (iy > height() - 1) {
        iy = height() - 1;
      }
    }
    float dx = fx - ix;
    float dy = fy - iy;
    float v00 = ReadRggb(ix, iy, ch);
    float v01 = ReadRggb(ix, iy2, ch);
    float v10 = ReadRggb(ix2, iy, ch);
    float v11 = ReadRggb(ix2, iy2, ch);
    float top_interp = v00 * (1 - dx) + v10 * dx;
    float btm_interp = v01 * (1 - dx) + v11 * dx;
    float interpolated = top_interp * (1 - dy) + btm_interp * dy;
    return interpolated;
  }

  // Same as above, but computes an RGB value instead of RGGB (by averaging the
  // two greens together).
  inline float InterpolatedReadRgb(float fx, float fy, int ch) const {
    switch (ch) {
      case 0: return InterpolatedReadRggb(fx, fy, 0);
      case 1: return (InterpolatedReadRggb(fx, fy, 1) +
                      InterpolatedReadRggb(fx, fy, 2)) * 0.5f;
      case 2: return InterpolatedReadRggb(fx, fy, 3);
      default: assert(false); return 0.0f;
    }
  }

  // Intended for internal Gcam use only, but clients could use this
  //   function if they are cropping their payload frames in any way
  //   *before sending them to Gcam*; in this case, it is critical
  //   to crop the SGM in the same way that the images were cropped.
  // The "crop_" values should all be in the [0..1] range, with
  //   crop_x0 < crop_x1, and crop_y0 < crop_y1.
  // Note that our YUV and raw pipelines have different interpretations of how
  //   the values in the SGM should be applied (spatially) to the pixels of the
  //   image; this means the SGMs will be cropped differently, in this function,
  //   depending on which pipeline is calling the function.  Therefore, be sure
  //   to pass in the actual value of 'process_bayer_for_payload' (even if this
  //   SGM is for a metering frame).  For more details, see the implementation.
  // Returns a new SpatialGainMap.
  // TODO(geiss): If we deprecate the YUV pipeline, remove this parameter.
  SpatialGainMap ResizeAndCrop(
      int new_width,
      int new_height,
      NormalizedRect crop,
      bool process_bayer_for_payload) const;

  // Returns true if the LSC map values look good, or false if there are any
  // potential issues.  The (optional) return string (details) describes the
  // problem, if the return value is false; otherwise, it will be set to an
  // empty string.
  bool Check(std::string* details) const;
  void Print() const;

  int width() const { return gain_map_.width(); }
  int height() const { return gain_map_.height(); }
  int num_channels() const { return gain_map_.num_channels(); }
  InterleavedImageF gain_map() const { return gain_map_; }

  bool is_precise() const { return is_precise_; }

  // Serializes the SpatialGainMap to a buffer of bytes (which can in turn
  // be written to disk.)  Note that this is the new, "right" way to save
  // SGMs to disk.  (On Glass, we also had some glue code that wrote to a
  // different (device-specific) format, and those files were named
  // "lsc_payload.bin".
  void SerializeToBuffer(std::vector<uint8_t>* buf) const;

  // Creates a SpatialGainMap from a binary blob.
  // Note that this is the new, "right" way to load SGMs.
  // (On Glass, the glue code used to write to a different (device-specific)
  //   format, and those files were named "lsc_payload.bin" and read in via
  //   LoadLsc() in mock_input.cc.)
  static SpatialGainMap CreateFromBuffer(const uint8_t* buffer, int byte_count);

  inline bool has_extra_vignetting_applied() const {
    return has_extra_vignetting_applied_;
  }

  // Scale the values of this SpatialGainMap by the values in scale_map, and set
  //   the flag 'has_extra_vignetting_applied_' to true.
  // The two maps must have the same dimensions.
  // In the unforced variant, the 'has_extra_vignetting_applied_' flag must be
  //   false before the call.
  void ScaleBy(const SpatialGainMap& scale_map) {
    assert(!has_extra_vignetting_applied_);
    ForceScaleBy(scale_map);
  }
  void ForceScaleBy(const SpatialGainMap& scale_map);

 private:
  InterleavedImageF gain_map_;

  // Whether the spatial gain map is a precise map from the ISP. If this flag
  // is false, the spatial gain map is a parametric guess.
  bool is_precise_ = false;

  bool has_extra_vignetting_applied_ = false;
};

// These functions produce pre-canned SpatialGainMaps.
// For early bringup, use the helper function GenParameterizedSpatialGainMap()
//   below, which estimates one for you; just don't forget to replace it,
//   later, with the real map from the ISP.
// Just to get started, you can use something like:
//   GenParameterizedSpatialGainMap(17, 13, 1.0f, 3.0f, 2.0f);
SpatialGainMap GenSpatialGainMap_Flat();
SpatialGainMap GenSpatialGainMap_Glass();
SpatialGainMap GenSpatialGainMap_Glass_ReducedLSC();
SpatialGainMap GenSpatialGainMap_GalaxyNexus();
SpatialGainMap GenParameterizedSpatialGainMap(
    int w,                    // Odd values preferred.
    int h,                    // Odd values preferred.
    float gain_at_center,     // Should be 1.0f.
    float gain_at_corner,     // Should be > 1.0f.
    float falloff_exponent);  // Likely > 1.

// Additional vignetting used to adjust the lens shading map, to make the
// corners in the final result relatively darker. The parameters correspond
// to the input parameters of GenParameterizedSpatialGainMap.
struct RawVignetteParams {
  // Returns true if the parameters would have no effect.
  bool IsIdentity() const;

  float scale_at_corner = 1.0f;   // Normally <= 1.
  float falloff_exponent = 1.0f;  // Normally >= 1.
};

// Lerp to support tuning interpolation.
RawVignetteParams Lerp(const RawVignetteParams& a, const RawVignetteParams& b,
                       float t);

// Modifies a SpatialGainMap by applying extra vignetting to it, i.e. by
//   decreasing the gain values near the edges.
// In general, this should only be done when processing raw frames.
// The SGM must be non-null.
void AdjustRawVignetting(const RawVignetteParams& raw_vignette_params,
                         SpatialGainMap* sgm);

// Applies BLS (black level subtraction) and LSC (lens shading correction, using
//   bilinear interpolation) to an already-demosaic'd "raw" image.
// When BLS is applied, the white_level is pinned - i.e. if the black level
//   (on a channel) is 64 and white_level is 1023, then [64..1023] will be
//   linearly remapped to [0..1023].  LSC is applied after that.
// The output pixel values are clamped only to [0..65535]; this code doesn't
//   care how many bits the input is, and the output clamping is only to make
//   sure the values don't overflow the per-pixel storage.
// Note that this function isn't highly optimized, and might be slow if applied
//   to high-resolution images.  (In practice, we only use this version for
//   ae_test, and not on-device.)
void ApplyBlsAndSgm(const SpatialGainMap& sgm,
                    const float rgb_black_levels[3],
                    int white_level,
                    const InterleavedWriteViewU16* img);

// This version is specialized to apply the SGM to a pair of images.  Both
//   images should be the same size.
// Note that this function isn't highly optimized, and might be slow if applied
//   to high-resolution images.  (In practice, we only use it for low-
//   resolution images.)
void ApplyBlsAndSgm(
    const SpatialGainMap& sgm,
    const float rgb_black_levels[3],
    int white_level,
    const InterleavedWriteViewU16* img1,
    const InterleavedWriteViewU16* img2);

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_METADATA_SPATIAL_GAIN_MAP_H_
