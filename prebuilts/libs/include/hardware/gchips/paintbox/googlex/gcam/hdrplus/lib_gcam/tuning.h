#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_TUNING_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_TUNING_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <array>
#include <cassert>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

#include "googlex/gcam/ae/ae_type.h"
#include "googlex/gcam/base/pixel_rect.h"
#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/tet_model.h"
#include "googlex/gcam/image/icc_profile.h"
#include "googlex/gcam/image_metadata/bayer_pattern.h"
#include "googlex/gcam/image_metadata/frame_metadata.h"
#include "googlex/gcam/image_metadata/spatial_gain_map.h"
#include "googlex/gcam/image_proc/color_saturation.h"
#include "googlex/gcam/image_proc/resampling_method.h"
#include "googlex/gcam/image_proc/row_artifacts.h"
#include "googlex/gcam/image_raw/raw.h"
#include "googlex/gcam/tonemap/tonemap_yuv.h"

namespace gcam {

class LogSaver;
struct Context;
struct ShotParams;

enum GcamRoundingMethod {
  GRM_ROUND_DOWN = 0,
  GRM_ROUND_NEAREST,
  GRM_ROUND_UP,
  GRM_UNKNOWN
};

// Provide a generic Lerp that can be specialized by parameter tuning types.
template <typename T>
T LerpTuning(const T& a, const T& b, float t) {
  return a * (1.0f - t) + b * t;
}

// A wrapper around std::map<float, T> that allows interpolating its values.
template <typename T>
class SmoothKeyValueMap {
  // It turns out that std::map has at least two to-be-identified bugs in the
  // Android STL implementation (see b/37967029), so this is actually
  // implemented with a (sorted) vector of pairs.
  static bool KeyValueLess(const std::pair<float, T>& l,
                           const std::pair<float, T>& r) {
    return l.first < r.first;
  }

  std::vector<std::pair<float, T>> map_;

 public:
  SmoothKeyValueMap() {}

  // Construct a smooth key-value map from an initializer list of key-value
  // pairs. The pairs should be sorted.
  SmoothKeyValueMap(std::initializer_list<std::pair<float, T>> pairs)
      : map_(pairs) {
    // To implement lookups, we need the keys to be sorted. They should be
    // sorted already for this to make sense at all.
    std::sort(map_.begin(), map_.end(), KeyValueLess);
  }

  // Perform a linearly interpolated lookup into this map. If the map is empty,
  // this returns a default constructed T. If key is outside the range defined
  // by the keys of the map, the function returns the nearest key (i.e. it does
  // not extrapolate values).
  T Get(float key) const {
    if (map_.empty()) {
      // If we have no values, return a default constructed value.
      return T();
    } else if (key <= map_.front().first) {
      return map_.front().second;
    } else if (key >= map_.back().first) {
      return map_.back().second;
    } else {
      std::pair<float, T> key_and_dummy(key, T());
      auto p2 = std::upper_bound(map_.begin(), map_.end(), key_and_dummy,
                                 KeyValueLess);
      // Get the previous datapoint, and interpolate.
      assert(p2 != map_.begin() && p2 != map_.end());
      auto p1 = std::prev(p2);

      assert(p1->first <= key && key <= p2->first);
      float t = (key - p1->first) / (p2->first - p1->first);
      return LerpTuning(p1->second, p2->second, t);
    }
  }
};

// Overload lerp to support tuning interpolation.
template <>
inline RawVignetteParams LerpTuning<RawVignetteParams>(
    const RawVignetteParams& a, const RawVignetteParams& b, float t) {
  return Lerp(a, b, t);
}

// TODO(yuntatsai): Reorganize noise model code.
// Description of the noise found in a particular raw/linear image. This model
// describes noise variance as a linear function of the ideal signal level,
// given as digital values of the input image after black level subtraction,
// in the range [0, white_level - black_level]. The model assumes the noise is
// spatially independent (white noise).
//
// Apart from the different units for signal, this model is identical to
// DngNoiseModel, which corresponds to the DNG specification for the
// 'NoiseProfile' tag.
struct RawNoiseModel {
  // The noise variance for a given signal level x is modeled as:
  //
  //   Var[x] = scale*x + offset
  //
  // where x is the noise-free signal level, expressed in digital values after
  // black level subtraction, in the range [0, white_level - black_level].
  float scale = 0.0f;
  float offset = 0.0f;

  // Produce a raw noise model from a DNG noise model and the white/black
  // levels.
  static RawNoiseModel FromDngNoiseModel(const DngNoiseModel &dng,
                                         float black_level, float white_level) {
    float normalize_factor = white_level - black_level;
    RawNoiseModel raw;
    raw.scale = dng.scale * normalize_factor;
    raw.offset = dng.offset * normalize_factor * normalize_factor;
    return raw;
  }
};

// The DNG noise model is specified independently for each Bayer channel, in
// terms of normalized signal levels. This function converts a 4-channel DNG
// noise model to black-subtracted raw noise models.
void RawNoiseModelFromDngNoiseModel(
    const DngNoiseModel dng_noise_model_bayer[4],
    const float black_levels_bayer[4], float white_level,
    RawNoiseModel* raw_noise_model);

// Noise model from PD is specified based on the noise model for the green
// channel. This function is only guaranteed to work with the 2017 phones.
// 'pd_noise_model' contains the noise model for the left and right PD pixels
// in that order.
void PdNoiseModelFromRawNoiseModel(
    const RawNoiseModel raw_noise_model[4],
    BayerPattern bayer_pattern,
    std::array<RawNoiseModel, 2>* pd_noise_model);

// Compute the average SNR for a given raw frame, by evaluating the given noise
// model at the mean signal level, approximated using a single green channel.
// Only a single value is used for black level.
float AverageSnrFromFrame(const RawReadView& raw,
                          BayerPattern bayer_pattern,
                          float noise_model_black_level,
                          float white_level,
                          const RawNoiseModel& noise_model,
                          const Context& context);

// Description of the noise found in raw/linear images captured by a
// particular sensor as a function of an analog gain stage followed by a readout
// stage, followed by digital gain. This model assumes the noise is spatially
// independent (white noise).
//
// For a given analog and digital gain, we get a noise model that describes the
// noise variance as a linear function of the ideal signal level, where the
// signal is normalized to the range [0, 1].
//
// For more information about the derivation and assumptions of this model, see
// this document:
// https://docs.google.com/a/google.com/document/d/1NLTbnZ6KIpLVnPanaOtGvFH1ym-6jWPBeWhOYqnAJBY     // NOLINT
//
struct SensorNoiseModel {
  // This model is implemented by defining two functions (lines a*x + b) of the
  // gain to determine scale and offset.

  // scale = scale_a*analog_gain*digital_gain + scale_b
  float scale_a = 0.0f;
  float scale_b = 0.0f;

  // offset = (offset_a*analog_gain^2 + offset_b)*digital_gain^2
  float offset_a = 0.0f;
  float offset_b = 0.0f;

  // Compute an image-specific noise model for an image captured at particular
  // gains described by a sensor described by this model.
  DngNoiseModel DngNoiseModelForGain(float analog_gain,
                                     float digital_gain) const {
    float analog_gain_sq = analog_gain * analog_gain;
    float digital_gain_sq = digital_gain * digital_gain;

    DngNoiseModel dng_noise_model;
    dng_noise_model.scale = scale_a * analog_gain * digital_gain + scale_b;
    dng_noise_model.offset =
        (offset_a * analog_gain_sq + offset_b) * digital_gain_sq;
    return dng_noise_model;
  }

  bool Check() const {
    // Note that scale_b might go negative, as a correction term for a sensor
    // with black level funkiness. In an ideal world, scale_b would be 0, as
    // signal-dependent noise variance (i.e. shot noise) should scale perfectly
    // with gain.
    return scale_a > 0.0f && offset_a >= 0.0f && offset_b >= 0.0f;
  }
};

// Description of the row noise found in raw/linear images captured by a
// particular sensor as a function of gain. This model describes row noise as
// variances of row sums. This model assumes that the row noise is additive
// (signal independent).
// TODO(dsharlet): Determine the proper relationship between analog gain,
// digital gain, and row noise variance. For now, I'm assuming that the noise is
// added prior to the analog gain.
// TODO(dsharlet): This is assuming that the noise is correlated within rows
// of the image, but this isn't true for (unusual) sensors rotated by 90
// degrees.
class SensorRowArtifacts {
 public:
  // This is a spectrum of variances that occur in the sum of the rows
  // when gain = 1. The variance is computed over an area with radius 2^n, where
  // n is the index in the vector. The variances are as measured on normalized
  // pixel values [0, 1].
  std::vector<float> noise_offset;

  // List of patterns to be detected and suppressed. Patterns introduced before
  // analog gain are affected by analog gain, patterns after analog gain are
  // not.
  std::vector<RowPattern> patterns_pre_analog_gain;
  std::vector<RowPattern> patterns_post_analog_gain;

  // Compute an image-specific row noise model for an image captured at
  // particular gains.
  std::vector<float> NoiseVariancesForGain(float analog_gain,
                                           float digital_gain,
                                           float black_level,
                                           float white_level) const {
    float gain = analog_gain * digital_gain;
    float normalize = white_level - black_level;

    std::vector<float> result(noise_offset.size());
    for (size_t i = 0; i < result.size(); i++) {
      result[i] = noise_offset[i] * gain * gain * normalize * normalize;
    }

    return result;
  }

  // Get a list of patterns expected to be found in an image with the applied
  // analog/digital gains.
  std::vector<RowPattern> PatternsForGain(float analog_gain,
                                          float digital_gain,
                                          float black_level,
                                          float white_level) const {
    float normalize = white_level - black_level;

    std::vector<RowPattern> result;
    for (const RowPattern& i : patterns_pre_analog_gain) {
      RowPattern result_i = i;
      result_i.amplitude *= analog_gain * digital_gain * normalize;
      result.push_back(result_i);
    }
    for (const RowPattern& i : patterns_post_analog_gain) {
      RowPattern result_i = i;
      result_i.amplitude *= digital_gain * normalize;
      result.push_back(result_i);
    }

    return result;
  }
};

// Hot pixels often vary in intensity with analog gain, this stores a set of
// key-value pairs of analog gains and thresholds, which are linearly
// interpolated to look up thresholds for a specific analog gain.
struct HotPixelParams {
  SmoothKeyValueMap<int> threshold;

  HotPixelParams();
};

// Get the hot pixel threshold, given the tuning and frame metadata.
int GetHotPixelThreshold(const HotPixelParams& params,
                         const FrameMetadata& meta);

// Per-device configurable tuning settings for raw image merging.
struct RawMergeParams {
  RawMergeParams();

  // If true, disable alignment completely. This causes merge to use zero
  // displacement alignments for all tiles.
  bool disable_align;

  // Mapping of average base frame SNR to tile sizes to use for align and merge.
  // The tile size actually used is rounded down to the previous power of 2.
  SmoothKeyValueMap<int> align_tile_size;
  SmoothKeyValueMap<int> merge_tile_size;

  // SNR dependent scalar factors on the expected amount of noise for each step
  // of the merge denoising filter. Increasing the amount of expected noise
  // increases the noise reduction strength.
  SmoothKeyValueMap<float> temporal_strength;
  SmoothKeyValueMap<float> spatial_strength;
};

// The number of frequencies used to describe the shape of the unsharp mask
// filter.
static const int kRawSharpenUnsharpMaskFreqs = 3;

struct RawSharpenParams {
  RawSharpenParams();

  // An overall scaling amount of the unsharp mask filter.
  float unsharp_mask_strength;
  // The maximum overshoot allowed as a fraction of the white level.
  float max_overshoot;
  // Describes the amplitude of the Gaussian low pass filters with varying
  // frequency for the unsharp mask. Let f0 be the frequency described by _f[0],
  // the highest possible frequency. Then the frequency described by _f[n] is
  // f0/2^n.
  float unsharp_mask_f[kRawSharpenUnsharpMaskFreqs];
};

// Chromatic aberration (CA) suppression is performed by assigning a probability
// of CA artifacts to each pixel, and attempting to adjust the chroma of pixels
// with high probability to reduce the appearance of CA artifacts.
struct ChromaticAberrationParams {
  // The radius of the neighborhood for detecting CA, in pixels. This roughly
  // corresponds to the maximum distance between a bright pixel and the CA
  // artifact it produces.
  int radius = 6;

  // A pixel considered to be potentially affected by chromatic aberration
  // (CA) artifacts if the local contrast is greater than luma threshold, and
  // the chroma of that pixel is less than the chroma threshold.
  float luma_threshold = 1.0f;
  float chroma_threshold = 0.0f;

  // If a pixel has high probability of being affected by CA, the chroma is
  // adjusted by an amount proportional to this suppression parameter.
  float suppression = 0.0f;
};

// Overload lerp to support tuning interpolation.
template <>
RawSharpenParams LerpTuning<RawSharpenParams>(const RawSharpenParams& a,
                                             const RawSharpenParams& b,
                                             float t);

// The struct records the arc flare that is present on marlin/sailfish when
// the main light source is at ~46-degree
struct ArcFlareParam {
  // specify if the arc flare can exist in this device
  bool can_exist;

  // Compute the mean arc flare radius. According to the calibration data, the
  // average radius largely depends on the focus distance. It is very linear
  // to the focus step, but unfortunately not available at the app level.
  // Here we fit a polynomial to the focus distance in diopters for it.
  // Assumption: the sensor dimension is the 2x2 binned, as the one used in
  // FinishRaw
  float GetMeanRadius(float focus_distance_diopters) const {
    const float fdd2 = focus_distance_diopters * focus_distance_diopters;
    const float fdd3 = fdd2 * focus_distance_diopters;
    return radius_param[0] + radius_param[1] *
        (radius_param[2] + radius_param[3] * focus_distance_diopters +
         radius_param[4] * fdd2 +
         radius_param[5] * fdd3);
  }

  double radius_param[6];
};

// Denoising is performed on a image pyramid frequency decomposition. At the
// highest frequency level, only luma denoising is performed, using a laplacian
// shrinkage algorithm. For the rest of the levels, both the luma and chroma
// are denoised using an approximated bilateral filter.
struct DenoiseParams {
  // Relative strength of the denoising. This value is used to scale the
  // expected noise standard deviation. Larger values increase the expected
  // noise standard deviation, which increases the denoising performed by the
  // filter. The array elements correspond to pyramid levels, indexed from the
  // base (highest resolution) pyramid level. The pyramid levels are decimated
  // by 2 at each level.
  float luma_strength[3] = { 1.0f, 1.0f, 1.0f };
  float chroma_strength[3] = { 1.0f, 1.0f, 1.0f };

  // This is used to blend back the original image, at each pyramid level.
  // A value of 0 means none of the original image is used; a value of 1
  // reverts to the original image completely.
  float revert_factor[3] = { 0.0f, 0.0f, 0.0f };
};

template <>
DenoiseParams LerpTuning<DenoiseParams>(const DenoiseParams& a,
                                        const DenoiseParams& b,
                                        float t);

struct RawFinishParams {
  RawFinishParams();

  // Extra vignetting to apply in the finish pipeline, specified as
  // interpolation parameters for a spatially varying lerp toward 1.0.  This
  // tuning can be useful if the vendor-provided lens shading correction is too
  // strong, e.g. in dark conditions.  Note that this extra vignetting is *not*
  // taken into account by AE; it is strictly post-processing.  The parameters
  // (values) for extra vignetting are specified as a function of the estimated
  // average SNR (keys) for the merged frame.
  SmoothKeyValueMap<RawVignetteParams> extra_finish_vignetting;

  // Whether to disable extra vignetting for ZSL shots. If this flag is set,
  // the extra vignetting will only take effect for non-ZSL (HDR+ ON) shots.
  bool disable_extra_vignetting_for_zsl;

  SmoothKeyValueMap<DenoiseParams> denoise;

  ChromaticAberrationParams chromatic_aberration;

  // Sharpen parameters are stored as key-value pairs of the estimated average
  // SNR of the image immediately prior to sharpening being applied.
  SmoothKeyValueMap<RawSharpenParams> sharpen_params;

  // Sharpening strength as a function of digital zoom. (Images are
  // sharpened less when digital zoom is applied.  In order to determine
  // the final amount of sharpening, get sharpen_params based on SNR, get
  // zoom_sharpen_attenuation based on digital zoom factor, and multiply
  // sharpen_params.unsharp_mask_strength by zoom_sharpen_attenuation.
  SmoothKeyValueMap<float> zoom_sharpen_attenuation;

  // Strength of sharpening after digital zoom as a function of the digital
  // zoom factor.
  SmoothKeyValueMap<float> post_zoom_sharpen_strength;

  // Which method to use when resampling the final image.
  ResamplingMethod resampling_method;

  // How much error to expect in the black level metadata, in DNs. If this is
  // greater than zero, we attempt to estimate an offset within the margin of
  // error.
  // TODO(dsharlet): The code that uses this assumes the white level is 1023.
  float max_black_level_offset;

  // A 3D RGB -> RGB mapping to apply to the image. The RGB values are
  // normalized (i.e. lie in [0, 1]). Prior to being applied to the image, this
  // will be converted to a YUV -> UV mapping (the change in Y will be ignored),
  // and quantized to a LUT.
  std::function<std::array<float, 3>(std::array<float, 3>)> rgb_map;

  // Parameters for color saturation to apply during finish.
  ColorSatParams saturation;

  // Biases to apply to the final RGB output color.
  // The values are normalized, so 1.0 corresponds to kRawFinishWhiteLevel.
  //   They can be positive or negative.  A value of -0.01, for example, would
  //   subtract all final pixels (on that color channel) by 1% of
  //   kRawFinishWhiteLevel.
  // Use of this feature is HEAVILY DISCOURAGED.
  float final_rgb_bias_hack[3];

  // If > 0, limits the maximum number of synthetic exposures in the HDR block.
  int max_synthetic_exposures;

  // Flare suppression parameters.
  ArcFlareParam arc_flare;

  // ICC profile to embed in the output, specifying the output color space.
  // NOTE: All tuning currently assumes an sRGB output color space.
  // TODO(hasinoff): Fork the tuning for color saturation and 3D LUT to take
  //   output color space into account.
  // TODO(hasinoff): Dither more heavily for wide gamut color spaces, to offset
  //   increased quantization in 8-bit output?
  IccProfile icc_profile;
};

// This struct houses a subset of the parameters for capture, and is limited
//   to the subset that we need to tune differently, when capturing for the
//   YUV vs. raw pipelines.
// Each device_code has one of these for the YUV pipeline, and one for the
//   raw pipeline.  You should select between them using
//   ShotParams::process_bayer_for_payload.
struct CaptureParams {
  // The default values assume raw payload processing.
  CaptureParams() { SetDefaults(/*process_bayer_for_payload=*/true); }

  bool Check() const;
  void SetDefaults(bool process_bayer_for_payload);

  // When true, in some HDR scenes, Gcam will capture a single *true* long
  // exposure, for improved color accuracy in the dark parts of the scene, at a
  // cost of one fewer short exposure and an extra processing step (in Finish).
  bool capture_true_long_exposure;

  // In non-ZSL mode, determines whether the sensor is allowed to apply digital
  //   gain to raw payload frames.  (Does not apply to ZSL mode, where the
  //   client decides what to capture; and in non-ZSL mode, it applies only
  //   to the capture of explicit payload bursts - not metering bursts.)
  // Disabling sensor-side digital gain can provide several advantages:
  //   1. Any digital gain applied to a raw image at the sensor will cause
  //      erroneous color shifts if the black levels are not exactly correct.
  //      If HDR+ will come up with better black levels than those used by
  //      the sensor, then waiting to apply digital gain (until we can use
  //      those improved black levels) will avoid these color shifts.
  //   2. Avoids the dangers of using digital gain (especially non-integer
  //      digital gains) on sensors that don't implement digital gain well,
  //      leading to re-quantization.  (The Pixel 2016 sensor, in its shipping
  //      configuration, has this issue; there, unfortunately, the driver
  //      tries to force a small amount of non-integer digital gain into any
  //      shot that uses analog gain.  This was done to limit color-popping
  //      artifacts near the edges of the viewfinder, but sadly, it causes
  //      extra quantization that causes ugly color banding - especially
  //      visible on low-noise images with a small bright center and very
  //      dark edges.)
  float allow_digital_gain_at_sensor;

  // Note that the limits below involving "gain" do not take into account
  // the extra digital gain from lens shading, white balance gains, or the CCM.

  // Maximum additional ("post-capture") digital gain applied by gcam.
  //   Any digital gain applied earlier, by the sensor, to the raw input
  //   frames, does not count toward this limit.
  // Post-capture gain includes:
  //   1. Global digital gain applied by gcam
  //   2. Local digital gain applied by gcam (during local tonemapping).
  // If necessary, either or both of these will be throttled in order to meet
  //   this constraint.
  // TUNING:
  //   This limit should be tuned primarily to limit artifacts from
  //     quantization in raw input frames.  (It should not be tuned based on
  //     noise or denoising limits, or to mitigate color shifts caused by
  //     imperfect black levels.)
  //   This limit won't often kick in when sensor-side digital gain is used,
  //     and in that case, can be kept low (say, 12).  When sensor-side digital
  //     gain is not used, this value should be higher, and should be more
  //     carefully tuned.
  float max_post_capture_gain_zsl;
  float max_post_capture_gain_non_zsl;

  // The max overall gain that Gcam will allow in the final image.
  // This includes:
  //   1. Analog gain at the sensor
  //   2. Digital gain at the sensor
  //   3. Global digital gain applied by gcam
  //   4. Further digital gain applied to the shadows, by gcam, for local
  //        tonemapping.
  // If necessary, the local (and even global) gain applied by gcam will be
  //   throttled in order to meet this constraint.
  // TUNING: This limit should be as high as possible, until either:
  //   1. Noise becomes too strong for our denoising algorithms, or
  //   2. Black level imperfections begin to produce unacceptable color shifts.
  //   This threshold should not, however, be tuned to mitigate issues from
  //   from quantization (such as color banding) in the incoming raw images.
  float max_overall_gain;

  // This limits the maximum dynamic range compression that our local
  //   tonemapping block can deliver (for a given device & pipeline). AE
  //   prescribes two TET values for a scene: a short TET and a long TET.
  //   Let 'hdr_ratio' be the ratio (long_tet / short_tet).  If hdr_ratio is
  //   less than 'max_hdr_ratio' then HDR can be used (with these exact TETs)
  //   on the scene.
  // If hdr_ratio exceeds max_hdr_ratio, then HDR can still be used, *but* the
  //   short or long TET will be adjusted, by blowing out the short exposure
  //   (increasing short_tet), dimming the long exposure (decreasing long_tet),
  //   or some combination of the two. In this case, the HDR ratio after
  //   adjustments will be exactly max_hdr_ratio.
  // TUNING: This limit should be tuned based on how far we can push our local
  //   tonemapping block to compress dynamic range, without producing cartoony
  //   renditions.  (It should not be tuned to mitigate noise, quantization in
  //   shadows, or color shifts from poor black levels.)
  // Note that if you increase this value, then for devices/modes that don't
  //   use sensor-side digital gain, you might also need to increase
  //   post_capture_digital_gain_[zsl/non_zsl], in order to see the full
  //   benefit on all shots.  Of course, don't force it; follow the 'TUNING:'
  //   guidelines for that limit.
  float max_hdr_ratio;  // Should be > 1.

  // *** The YUV pipeline ignores this member. ***
  // In the raw pipeline (only), this value controls the ratio between the
  //   variance of the noise in a single captured frame (at the time it goes
  //   into merge - i.e. after analog and digital gains are applied, but before
  //   HDR), and the number of payload frames we would ideally like to merge.
  // A higher value will cause more frames to be captured & subsequently merged.
  // The basic formula is:
  //   <desired # of raw payload frames to merge> =
  //       round(<normalized variance> *
  //             noise_variance_to_payload_frame_count *
  //             hdr_ratio)
  // For reference, Nexus 6 frames have a variance of about 4e-5 when analog and
  //   digital gain are both 1.  So if noise_variance_to_payload_frame_count is
  //   7e4, then in a broad daylight LDR scene, it would capture ~2.8 frames
  //   (which would round to 3).
  float noise_variance_to_payload_frame_count;
};

// This structure contains all of the settings for Gcam for a certain type
// of device (such as Glass 1.0, some particular phone model, etc).
//
// The 'device_code' string should uniquely identify the device (and software
// version) but can not contain any underscores.
//
// Although we don't yet serialize the per-device tuning, by bumping the
// device code with each significant tuning change, the versioned device code
// lets us look up a snapshot of the tuning. This helps us understand the
// per-device configuration (capture tuning, parameters for image processing)
// for historical bursts. When reprocessing such bursts, we may want to
// override this historical tuning, however, some parts of this tuning
// (e.g. Tuning::sensitivity, which controls autoexposure) is irrevocably
// baked in to the saved images themselves.
//
// **** TO GET STARTED: ****
//   Simply call GetTuningFromDeviceCode() using "uncalibrated" as device_code.
//   This will give you good initial values that you can then start to refine.
//
struct Tuning {
  // Return whether the tuning is valid, according to some basic error checks.
  bool Check() const;

  // A string identifying the capture device, and a software version
  //   that changes each time the capture settings or ISP processing
  //   change.
  // The string must not contain any underscores.
  // Best practice is to have the beginning of the string identify
  //   the device (SKU), and the later part of the string identify
  //   the version string of the "software".  Then, any time the nature
  //   of the captured images changes, you should bump that version
  //   string.
  // For example, you could use "supernexus101a" for a phone called the
  //   "Super Nexus", with software version 1.01a; then bump it to
  //   "supernexus101b" on a minor sensor/ISP change, or "supernexus102"
  //   on a more major change.
  // Ideally, for reprocessing older bursts (which were captured with
  //   different settings), you want to have a different Tuning structure
  //   for each older version of the software, and this string helps
  //   you identify which software was used to capture a (saved) burst,
  //   so you can reproduce the right Tuning structure for it, and
  //   [properly] reprocess it.
  // So, you'll also want to build a function around that produces your
  //   Tuning structure, given any "device_code" that you've used in the
  //   past.
  // Changes that warrant bumping the version number include:
  //   1. Changes to sensor register settings that result in different
  //      image properties (noise, sharpness, black level, etc) which
  //      would then warrant different tuning parameters in Gcam.
  //   2. Changes to how the ISP processed the images (changed RGB->YUV
  //      matrix; found bug where wrong tonemapping curve was being used;
  //      different demosaic; etc.).
  std::string    device_code;  // Must not contain "_" character.

  // Input-oriented data:
  // -------------------------------------
  // (Note: We exclude the NoiseModel for YUV images here because it
  //  usually comes from a file, whereas - at least for reprocessing -
  //  the stuff here can usually be generated from just a device_code
  //  (when known).)

  // This tells Gcam how sensitive your device's camera module is to light,
  //   when capturing an image with minimal (usually no) gain.
  // For this, we want the sensitivity of the sensor and the lens, together.
  // Use the following formula:
  //   [sensitivity] = [ISO at min gain] / ([f-number] ^ 2)
  // About "ISO at min gain":
  //   You will have to measure [ISO at min gain], otherwise known as
  //   "minimum ISO", using the ISO 12232:2006 REI method (note that this is
  //   the same requirement used by the Android CameraHAL3 spec).  Be sure
  //   to follow the spec exactly.  (Note that the [ISO at min gain] value
  //   is probably NOT a nice even number like 50 or 100, except on rare
  //   coincidences.)  For example, for Glass v1, this value is about
  //   67.5.  But note that a higher (or lower) value here does NOT necessarily
  //   mean that a sensor captures more (or less) signal!  ISO only establishes
  //   a relationship between scene brightness and pixel brightness, normalized
  //   for lens speed; it does not say anything about how noisy that signal is.
  // About "sensitivity":
  //   The [ISO at min gain] metric measures sensor speed only, factoring
  //   out the aperture size.  To get the sensitivity of the sensor and lens,
  //   together, you must divide [ISO at min gain] by the f-number, squared.
  //   (You should have multiplied by [f-number]^2 when computing
  //   [ISO at min gain], to factor out the effect of aperture, so really,
  //   this is just undoing that, leaving the aperture factored in.)
  //   For example, for Glass v1, sensitivity = 67.5 / 2.48^2 = 10.975.
  // Precision required:
  //   This value must be accurate to within 20%, otherwise you could
  //   experience problems with over-exposure of extremely bright scenes
  //   (such as super-bright high-altitude sunny-day snow scenes).
  // How is the value used?
  //   This value is currently used to determine the minimum exposure
  //   time Gcam might need to use, to cover all reasonable earthly scenes.
  //   Someday, Gcam's AE (auto exposure) might also use it to match observed
  //   image brightness levels to real-world brightnesses, and to do a better
  //   job of exposing the scene.
  float sensitivity;

  // This describes the noise that occurs in the raw measurements from the
  //   sensor, for given capture settings and ideal signal level. This noise
  //   model is only useful for raw images; it is invalidated by the processing
  //   required to produce a YUV image.
  // There are four separate SensorNoiseModel's for each color channel of the
  //   raw image, following the Bayer plane order defined by the top-left 2x2
  //   pixels of the sensor, in row-column scan order (or "reading" order):
  //   upper-left, upper-right, lower-left, and lower-right.
  // NOTE: This tuning overrides FrameMetadata::dng_noise_model_bayer[], but it
  //   should only be necessary for older devices, or devices with untrustworthy
  //   metadata.
  SensorNoiseModel sensor_noise_model_override_bayer[4];

  // This describes the row noise that occurs in the raw measurements from the
  // sensor.
  SensorRowArtifacts sensor_row_artifacts;

  // The input (forward) tonemapping curves.
  // The client must use these curves when capturing *metering or payload*
  //   frames.
  // The client is free to use different curves when capturing *viewfinder*
  //   frames (that are fed into Gcam), which will likely be different,
  //   as long as they are reported to Gcam::AddViewfinderFrame().
  //
  // **** The client should not modify these fields directly.      ****
  // **** Instead, only set the input tonemapping curves through   ****
  // **** calls to SetInputTonemap(), below.                       ****
  TonemapFloat input_tonemap_float;  // Control points in [0,1] x [0,1].
  Tonemap      input_tonemap;        // LUT from 10 bits to 8 bits.
  RevTonemap   input_rev_tonemap;    // LUT from 8 bits to 10 bits.

  // 2. Capture-oriented parameters:
  // -------------------------------------

  // Parameters that affect the capture of a YUV or raw payload, respectively.
  CaptureParams yuv_payload_capture_params;
  CaptureParams raw_payload_capture_params;

  // The max analog gain that Gcam *should use*.
  // This is *not* necessarily the highest analog gain that your sensor
  //   supports, but rather, the highest value you want Gcam to use.
  // Usually 8 or 16.
  // This field doesn't make sense in the context of Android, where the Camera2
  //   API only lets you specify a desired sensitivity, and the breakdown into
  //   analog/digital gain is handled below the HAL level.
  // TODO(ruiduoy): Remove this field, and change the semantics of max analog
  //   gain to mean the highest supported by the sensor, i.e. computable as
  //   (StaticMetadata::max_analog_iso / StaticMetadata::iso_range[0]).
  float    max_analog_gain;

  // Get the minimum exposure time that Gcam should use for its payload
  //   frames based on the camera's sensitivity.
  // Also applies to the metering frames, if no binning is used.
  //   If binning is enabled during metering (only) (which results in lower-
  //   resolution but brighter frames), then the minimum exposure time
  //   (for metering frames only) will be reduced proportionally.  For more
  //   information on this, see 'metering_frame_brightness_boost' in gcam.h.
  // This was calibrated on v1 Glass so that the min exposure time
  //   was just short enough to capture the brightest snowy scene
  //   we've ever seen, plus a small factor of safety.
  // Note: This must be low enough to capture the bright scenes,
  //   but we don't want it too low; otherwise, the metering
  //   burst would be covering a larger range, and (assuming
  //   you're not using tons of metering frames) the quality of
  //   AE would suffer.
  float     GetMinExposureTimeMs() const;

  // The maximum exposure time that Gcam should use for its payload
  //   frames.
  // If this is too short, then you won't be able to capture as many darker
  //   scenes.
  // If this is too long, then it will be hard to get clear shots in
  //   low light, due to hand shake.
  // Also applies to the metering frames, if no binning is used.
  //   If binning is enabled during metering (only) (which results in lower-
  //   resolution but brighter frames), then the maximum exposure time
  //   (for metering frames only) will be reduced proportionally.  For more
  //   information on this, see 'metering_frame_brightness_boost' in gcam.h.
  float    max_exposure_time_ms;

  // Whether to adjust exposure time to counteract banding artifacts, when
  //   flickering scene illumination is detected.
  bool     apply_antibanding;

  // This model controls how Gcam balances the use of longer exposure times
  //   vs. higher gain.
  // This applies to payload frames only.
  // There are two such models, for a given device; the selection of which one
  //   will be used is based on ShotParams::process_bayer_for_payload.
  TetModel yuv_payload_tet_model;   // TetModel for YUV payloads.
  TetModel raw_payload_tet_model;   // TetModel for raw payloads.

  // Determines the number of frames at the beginning of the payload burst
  //   that are deemed 'untrustworthy' and should (ideally) be excluded from
  //   selection as the base frame, inclusion in the AWB-averaging
  //   calculations, etc.
  // A well-tuned device should set this value to 0, to keep the latency for
  //   the base frame as low as possible.  However, during bringup, if the
  //   first 1 or 2 frames in the payload burst suffer from incorrect (or
  //   different) black levels, white balance, etc., you might want to
  //   temporarily block them from being base frame candidates.
  // Applies to non-ZSL shots only.
  // Range: [0+]
  int      fickle_payload_frames;

  // These values let you decide how many total frames should be considered
  //   for selection as the base frame.
  // The number of candidates should be large enough to yield sharp images
  //   (via lucky imaging), but small enough that you keep the average
  //   time-to-shot, and the time-to-postview, low.
  // Range: [1+]
  // A smaller value will always improve time-to-postview by that many frames,
  //   and will improve time-to-shot (the time until the capture of the frame
  //   that ends up as the base frame) by about half as much.  The downside is
  //   that it can result in softer photos (on average).  (Careful analysis
  //   should be done whenever tuning these values, simulating on data taken
  //   from thousands of shots.)
  // In general, we've found that Glass shots tend to be very stable, so you
  //   don't need as many candidate frames in order to get a sharp shot.
  //   On a handheld device, though, there is a lot more (hand) motion.
  // There are two different thresholds - one for "bright" scenes and one for
  //   "dark" scenes.  (But note that these labels are used for convenience
  //   and are not necessarily very accurate, since the cutoff is based on
  //   exposure time only, and not gain.)
  // 'base_frame_candidate_exposure_time_cutoff' is used to classify the scene
  //   as bright or dark, and from there, the appropriate threshold is used.
  // Applies to non-ZSL shots only.
  int      base_frame_candidates_in_bright_scene;
  int      base_frame_candidates_in_dark_scene;
  float    base_frame_candidate_exposure_time_cutoff_ms;

  // 3. Processing-oriented parameters:
  // -------------------------------------

  // Additional vignetting used to adjust the vendor-provided SpatialGainMap,
  //   when processing raw images. This makes the corners in the final result
  //   relatively darker, and the effect is taken into account by AE. (It has no
  //   effect when processing YUV images.)
  // This vignetting applies universally, to *all* scenes.
  // By contrast, the vignetting specified in
  //   RawFinishParams::extra_finish_vignetting is a function of SNR and is
  //   ignored by AE.
  //
  // When Gcam receives a SpatialGainMap, that map encodes the information
  //   needed to do two things:
  //     1. Fully correct any lens *color* shading, and
  //     2. *** Fully or partially *** correct all *vignetting*.
  // We always want to fully correct the lens color shading, but we often
  //   don't want to fully correct the lens vignetting.
  // Therefore, in the case where the SGM *fully* corrects all vignetting (the
  //   Camera2 spec does not require this, but some cameras seem to do it),
  //   it's very likely that, if applied to a raw image, the results would not
  //   match a YUV image from the ISP.  This is because, in reality, a small
  //   amount of vignetting looks nice, so ISPs usually leave some vignetting in
  //   the image.
  // To tune it:
  //   1. Use a plain white wall with even illumination as your test scene.
  //   2. First, tune 'scale_at_corner' until the vignetting of the final shot
  //      matches, *just at the corners*, between the ISP's YUV output, and
  //      the Gcam raw pipeline output.  (A larger value means more vignetting
  //      will be applied.)
  //   3. Second, adjust 'falloff_exponent' until the results of the Gcam raw
  //      pipeline maximally match the ISP's YUV output, but this time, not just
  //      at the corners, but everywhere.  (A larger value means a sharper
  //      falloff for the vignetting you're adding in.)
  RawVignetteParams raw_global_vignetting;

  // For these members, for starters, use the values from
  //   GetTuningFromDeviceCode("uncalibrated").
  // Later, you can work with the Gcam team to fine-tune these parameters
  //   for optimal image quality.
  HotPixelParams    hot_pixel_params;
  RawMergeParams    raw_merge_params;
  RawFinishParams   raw_finish_params;
  ColorSatParams    output_color_sat_yuv;

  // If true, then we will ignore any black (optically shielded) pixels
  //   specified in StaticMetadata (or their overrides in
  //   'black_pixel_area_override').
  // Initial black levels for each frame come normally come from
  //   FrameMetadata::black_levels_bayer[], but if black pixel areas are
  //   specified, and we're not ignoring them via this flag, then we'll use
  //   the black pixels, instead, to determine the initial black levels for
  //   each frame.
  // In either case, the resulting black levels might then be slightly refined
  //   (see 'max_black_level_offset').
  bool            ignore_black_pixels;

  // If this rectangle is valid, it overrides 'optically_black_regions' in
  //   StaticMetadata.
  // This rectangle must not overlap the active area, and in general only
  //   corresponds to a subset of the non-active pixels.
  // The rectangle is defined in the coordinates of the full pixel array.
  PixelRect       black_pixel_area_override;

  // [DEPRECATED]
  // Maximum overall gain supported by the *sensor* itself, including analog
  // and digital gain, or zero if unknown.
  //
  // Some sensors don't support digital gain. If the vendor applies digital gain
  // in the ISP, it may only be reflected in YUV/JPG images, and *not* in raw
  // images. Since Camera2 doesn't provide a breakdown of applied gain into
  // analog, sensor digital, and ISP digital gain, in such cases, we need to use
  // this field to correct the metadata that's reported for applied gain.
  //
  // This field is used *only* by the Nexus 5X front camera. Newer devices
  // separately report gain applied to raw (FrameMetadata::actual_analog_gain
  // and FrameMetadata::applied_digital_gain) and post-raw digital gain
  // (FrameMetadata::post_raw_digital_gain), so no metadata correction is
  // needed.
  float           max_raw_sensor_gain;

  // 4. Initialization functions:
  // -------------------------------------

  // Pass in the actual tonemapping (gamma) curve used by the ISP here,
  //   specified either as a set of floating point control points, or
  //   as a lookup table mapping 10-bit linear input to 8-bit tonemapped
  //   output. If a floating point tonemapping curve is provided, the client
  //   must also specify a rounding method to use (down, nearest, up) when
  //   converting to an integer lookup table.
  // Ideally, the client got this curve by calling one of the helper
  //   functions, like GenGcamTonemap() or GenGcamNexus5TonemapFloat(), and
  //   also passed the same curve to the camera HAL.
  // Each of the SetInputTonemap() initialization functions populate both
  //   representations of the input tonemapping curve, and generate the
  //   corresponding reverse tonemapping curve as well:
  //
  //     input_tonemap_float - floating point control points
  //     input_tonemap       - 10-bit to 8-bit lookup table
  //     input_rev_tonemap   - 8-bit to 10-bit lookup table (reverse)
  //
  //   In the version of SetInputTonemap() that takes Tonemap& as input,
  //   input_tonemap_float will be constructed densely, with one value for
  //   each index in Tonemap.
  bool SetInputTonemap(const TonemapFloat& input_tonemap_float,
                       const GcamRoundingMethod rounding_method);
  bool SetInputTonemap(const Tonemap& input_tonemap);

  // For Gcam's internal use.
  inline float GetMinTet() const { return GetMinExposureTimeMs(); }
  inline float GetMaxTet(bool process_bayer_for_payload) const {
    float max_overall_gain = GetMaxOverallGain(process_bayer_for_payload);
    return max_exposure_time_ms * max_overall_gain;
  }
  float GetMaxTet(const ShotParams& shot_params) const;

  inline const ColorSatParams& GetColorSatAdj(bool raw) const {
    if (raw) {
      return raw_finish_params.saturation;
    } else {
      return output_color_sat_yuv;
    }
  }

  inline float GetMaxOverallGain(bool process_bayer_for_payload) const {
    return GetCaptureParams(process_bayer_for_payload).max_overall_gain;
  }
  float GetMaxOverallGain(const ShotParams& shot_params) const;

  inline const CaptureParams& GetCaptureParams(
      bool process_bayer_for_payload) const {
    return process_bayer_for_payload
        ? raw_payload_capture_params
        : yuv_payload_capture_params;
  }
  const CaptureParams& GetCaptureParams(const ShotParams& shot_params) const;
};

// Gets tuning for the given device code and sensor ID.
// If the tuning does not exist for the given sensor ID, or if the device code
//   was not found and 'allow_unknown_devices' is false, returns false.
// If the device code was not found and 'allow_unknown_devices' is true,
//   warns and gets "uncalibrated" tuning.
bool GetTuningFromDeviceCode(const std::string& device_code, int sensor_id,
                             bool allow_unknown_devices, Tuning* tuning);

// Check whether the given FrameMetadata and Tuning are consistent.
bool CheckMetadataTuningConsistency(const FrameMetadata& meta,
                                    const Tuning& tuning,
                                    bool silent,
                                    LogSaver* log_saver);  // Optional.

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_TUNING_H_
