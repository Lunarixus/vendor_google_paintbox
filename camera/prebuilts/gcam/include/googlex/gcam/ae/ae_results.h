#ifndef GOOGLEX_GCAM_AE_AE_RESULTS_H_
#define GOOGLEX_GCAM_AE_AE_RESULTS_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <cstdint>
#include <vector>

#include "googlex/gcam/ae/ae_shot_params.h"
#include "googlex/gcam/ae/ae_type.h"
#include "googlex/gcam/image_metadata/awb_info.h"
#include "googlex/gcam/image_metadata/flash.h"
#include "googlex/gcam/image_metadata/frame_metadata.h"

// Assorted publicly-visible lightweight types used by the Gcam pipeline, and
// some associated utilities.

namespace gcam {

// Constants related to motion detection, based on frames fed to AE.
const float kMinMotionScore = 0.0f;
const float kMaxMotionScore = 100.0f;
const float kInvalidMotionScore = 999.0f;

// AE results for a given AE mode (single, short, long).
struct AeModeResult {
  // The TET (total exposure time, which is the product of exposure time, in
  //   milliseconds, with analog and digital gain factors) recommended by this
  //   isolated AE instance, for this scene.
  float tet = 0.0f;

  // The target "average LDR value" (or 'T' value, for short) that AE
  //   recommended for the scene. Think of this as the average pixel value in
  //   the final, rendered image (but with some biases based on color and real
  //   scene brightness; see SplitHdrImage::TetToAvgLdrValue(), where the
  //   value is computed, for more info).
  // Note: AE works by first comparing the scene's histogram to many others,
  //   to determine what this value should be; it then figures out the TET that
  //   will deliver this exact T value.
  float target_avg_ldr = -999.0f;

  // The confidence of the result; range: [0..1000].
  // About 5 or lower is generally a weak match; 10 is a good match; 20+ is a
  //   strong match.  It's rare to get anything over 30; the score is based on
  //   the inverse of (effectively) the earth-mover's distance between two
  //   histograms, and it's quite rare to encounter a histogram that strongly
  //   matches one you've seen (tagged) before.
  float confidence = -999.0f;

  // An absolute measure of the brightness of the scene.
  // For information on the range of values and what they mean, see comments
  //   above ShotLogData.
  // Each AE instance produces a (potentially different) 'log_scene_brightness'
  //   value.  The 3 log_scene_brightness values for the 3 instances potentially
  //   differ from each other because they are based on scene histograms that
  //   are a function of CWA maps, and the CWA can differ between the 3
  //   instances.
  // In practice, though, we standardize on using the log_scene_brightness value
  //   for the kHdrShort AE instance, to represent the scene - which is
  //   the one returned by AeResults::logSceneBrightness().
  float log_scene_brightness = -999.0f;
};

// Extra information returned with AeResults, for debugging, logging, and
// internal use.
struct AeDebugInfo {
  // How long it took to run AE for the HDR cases (kHdrShort,
  //   kHdrLong), and the non-HDR case (kSingle), respectively, in
  //   seconds.
  float exec_time_dual_ae_sec = 0;

  // The original AE results from each AE instance.
  AeModeResult original_result[kAeTypeCount];

  // The overall capture gains and noise models of the frames used to produce
  //   the AE results.
  // This can be useful to help estimate the noise model for a new capture
  //   setting, if a full sensor noise model (as a function of analog and
  //   digital gains) is unknown.
  std::vector<float> metering_frame_capture_gains;
  std::vector<DngNoiseModel> metering_frame_noise_models;
};

struct AeResults {
  // Are these results valid?
  // TODO(hasinoff): Replace this state with a Check() function.
  bool valid = false;

  // An absolute measure of the brightness of the scene.
  // Based on the kHdrShort AE instance, which we generally use as the
  //   standard value for the scene.
  // For information on the range of values and what they mean, see comments
  //   above ShotLogData.
  float LogSceneBrightness() const {
    return debug.original_result[kHdrShort].log_scene_brightness;
  }

  // Ideal HDR ratio.
  float IdealHdrRatio() const {
    return debug.original_result[kHdrLong].tet /
           debug.original_result[kHdrShort].tet;
  }

  // Final HDR ratio, after adjustments are made.
  float FinalHdrRatio() const {
    float final_hdr_ratio = final_tet[kHdrLong] / final_tet[kHdrShort];
    assert(final_hdr_ratio >= 1.0f);
    return final_hdr_ratio;
  }

  // The predicted average brightness [0..255] of the Gcam shot.
  // This corresponds to the expected simple average pixel value for the scene
  //   exposed at debug.final_tet[kHdrLong]. kSingle isn't an option
  //   because it doesn't always run. kHdrLong is preferred over
  //   kHdrShort since a larger fraction of the pixels almost always come
  //   primarily from the long exposure.
  // It is recommended that the caller use this value, together with
  //   LogSceneBrightness() (since this value can occasionally be low even for
  //   a bright but high-contrast scene), to determine whether flash should be
  //   used for the shot. In particular, we recommend triggering the flash if
  //   (predicted_image_brightness < 68.0f && LogSceneBrightness() < 3.5f).
  // TODO(geiss): Move these thresholds into named public-API constants.
  // TODO(geiss): Also discuss the suggested LogSceneBrightness() thresholds at
  //   the bright end of Auto-HDR+ (-0.3f and -1.0f, for the front and rear
  //   cameras respectively), and move these into named public-API constants as
  //   well.
  float predicted_image_brightness = 0.0f;

  // Estimate of *scene* motion during viewfinding, excluding slow panning or
  //   minor hand shake, computed when running background AE.
  // The motion score is only valid if MotionValid() returns true. Valid motion
  //   scores are in the range of kMinMotionScore (no motion) to kMaxMotionScore
  ///  (very high motion).
  // If the caller uses this to decide whether or not to trigger HDR+ in daytime
  //   HDR scenes when in Auto-HDR+ mode, we recommend triggering HDR+ if
  //   (MotionValid() && motion_score <= 1.0f).
  // Note that this signal *might* not work as well in very dark scenes, due to
  //   noisy raw viewfinder frames - although in casual testing, it seems to
  //   work fine.  For more information, see comments in
  //   SplitHdrImage::EvaluateMotion.
  // TODO(geiss): Move the threshold into a named public-API constant.
  float motion_score = kInvalidMotionScore;
  bool MotionValid() const {
    return motion_score >= kMinMotionScore && motion_score <= kMaxMotionScore;
  }

  // The final TET for the 3 instances of AE, after any adjustments.
  // Use this to capture the scene.
  // There are three possible adjustments relative to the original TET:
  //   1. Exposure compensation is applied.
  //   2. For HDR scenes where the original HDR ratio is beyond max_hdr_ratio,
  //      we blow out the highlights and/or darken the shadows as needed,
  //      relative their ideal levels, to prevent the hdr_ratio from exceeding
  //      max_hdr_ratio.
  //   3. For ZSL capture, we need to clamp the AE results to be at least as
  //      bright as the TET used to capture the base frame.
  // Note that final_tet[kSingle] might sometimes be 0 (invalid), unless you set
  //   the 'force_single_ae' flag.
  float final_tet[kAeTypeCount] = {0};

  // The AeShotParams that were used when running AE.
  // If an actual shot is taken using these AE results, then these AeShotParams
  //   should be used for shot_params.ae, when calling Gcam::StartShotCapture.
  AeShotParams ae_shot_params;

  // Describes the auto white balance for the scene, as a function of exposure.
  TetToAwb tet_to_awb;

  // The flash mode that should be used during the capture of the payload burst,
  //   if these AE results are used for a shot.
  FlashMetadata flash = FlashMetadata::kUnknown;

  // A (very) rough estimate of the % of pixels that would likely come from
  //   the long exposure, in an HDR shot.
  // Needed to convert these AE results into a payload burst spec.
  float fraction_of_pixels_from_long_exposure = 0.0f;

  // Whether the scene appears to be flickering, and its estimated frequency,
  //   based on the related metadata, FrameMetadata::scene_flicker, for the
  //   frames used for AE.
  // Needed to convert these AE results into a payload burst spec.
  SceneFlicker scene_flicker = SceneFlicker::kUnknown;

  // The timestamps of the frames used to produce the AE results.
  // This lets the client verify that the AE results correspond to the
  //   correct frames.
  std::vector<int64_t> metering_frame_timestamps_ns;

  // Number of metering frames used to produce the AE results.
  int MeteringFrameCount() const {
    return metering_frame_timestamps_ns.size();
  }

  // Extra information for debugging, logging, and internal use.
  AeDebugInfo debug;
};

}  // namespace gcam

#endif  // GOOGLEX_GCAM_AE_AE_RESULTS_H_
