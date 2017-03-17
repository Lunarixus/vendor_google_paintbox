#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_SHOT_LOG_DATA_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_SHOT_LOG_DATA_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <string>
#include <vector>

#include "hardware/gchips/paintbox/googlex/gcam/base/log_level.h"

namespace gcam {

struct AeResults;

struct Point2i {
  int x;
  int y;
};

// A lightweight struct that we return, at the end of each shot, to the
//   camera app, so that it can log extra statistics, if desired.
// We try to log only extra stuff (not already in the EXIF data) here.
// In addition, this collection of data is meant to be convenient; it's okay
//   if there are items in here that the caller could figure out by other
//   means; we just want to conveniently package them up here, to encourage
//   logging of the statistics we care about.
// Note: If you add or change members here, be sure to update the java wrapper
//   for the Google camera app for Android:
//   java/com/google/android/apps/camera/branches/kenai/src/com/android/camera/hdrplus/GcamShotStats.java   //NOLINT
struct ShotLogData {
  ShotLogData() { Clear(); }
  void Clear();
  bool Check() const;
  void Print(LogLevel log_level) const;
  void SerializeToString(std::string* str) const;

  // Did the shot trigger the HDR or non-HDR code path?
  bool hdr_was_used;

  // These values, in the [0..1000] range, that tell us how confident the AE
  //   algorithm was in its output for the 3 TETs.  A higher value is more
  //   confident.
  // A value of -1 indicates that that type of AE was not run executed.
  //   (This can happen for 'ae_confidence_single_exposure' when hdr_was_used
  //   is true).
  float ae_confidence_short_exposure;
  float ae_confidence_long_exposure;
  float ae_confidence_single_exposure;

  // The next two values describe the dynamic range of the scene in terms
  //   of a factor relating the brightness of the scene's shadows and its
  //   highlights (in linear terms, before any tone mapping). This factor
  //   can also be thought of as the overall compression that's needed to
  //   the fit the scene's brightness range (via local tone mapping) into
  //   an 8-bit, or "low-dynamic-range" (LDR) image.
  // By (hypothetically, at least) capturing a scene with two different
  //   exposures rather than one, then combining them with local tone
  //   mapping, we preserve detail from the shadows (using the longer
  //   exposure), prevent highlights from blowing out (using the shorter
  //   exposure), and effectively compress the dynamic range of the scene.
  // There are two versions (below) of this compression factor.
  // The first is the "ideal" compression factor that we would apply to the
  //   scene, if we had no limitations, and just wanted to force-compress
  //   the scene to fit in an LDR photograph.  This value is usually around
  //   1 for LDR scenes, and above ~1.5 for HDR scenes.  (Technically, this
  //   value is computed by taking the ideal long TET divided by the ideal
  //   short TET.)
  // The second is the "actual" compression factor that we (effectively)
  //   applied.  This is sometimes equal to the "ideal", and sometimes
  //   (for various technical and artistic reasons) it is less.
  // Expected values:
  //   hdr_was_used    ideal_range_compression   actual_range_compression
  //    false          ~[1 .. ~1.5] or [~6.5+]            1.0
  //    true               [~1.5 ... ~6.5]         [~1.5 ... ~3.5]
  float ideal_range_compression;
  float actual_range_compression;

  // This gives the fraction of pixels [0..1] that were unclipped (< 255
  //   in all 3 color channels) at the *ideal* long-exposure TET (before any
  //   adjustment factor was applied).  Always valid.
  float fraction_of_pixels_from_long_exposure;

  // If the ideal TET of the short or long exposure were adjusted, how were
  //   they adjusted?  These values tell you how they were scaled.  The values
  //   can be below 1 (dimmed), 1 (no adjustment) or above 1 (brightened).
  // If 'hdr_was_used' is false, then these values are not applicable, and
  //   will both be -1.
  float short_exp_adjustment_factor;
  float long_exp_adjustment_factor;

  // A measure of the (natural log of the) average brightness of the
  //   scene (after any digital zoom), extracted from the metering burst.
  // This 'scene brightness' metric is an absolute measure of how bright the
  //   objects in the scene are, taking capture settings into account.  It is
  //   supposed to (eventually) also take into account the sensitivity of
  //   the capture device, ***BUT*** it does not do this yet.  This is OK for
  //   now, as the sensitivity values of the devices we run on are all very
  //   similar; but it should be fixed at some point, especially if we ever
  //   add a device that is far more, or far less, sensitive.
  // TODO(geiss): Plumb the device's sensitivity value into
  //   HdrImageToWeightedAeSamples(), so that we can divide
  //   'denormalization_factor' by it.  (Then run an AE cross-validation
  //   and confirm that the error metrics went down, not up.)
  // TODO(geiss): Calibrate a mapping from log_scene_brightness to nits, then
  //   show lux in the ae_test viz, to make it more informative.
  // The values map to real-world scenes as follows:
  //    7.5  bright snow
  //    6.5  bright day / snow
  //    5.5  bright day
  //    4.5  day
  //    3.5  shade / near sunset
  //    2.5  deeper shade / after sunset
  //    1.5  indoor day / deep shade
  //    0.5  indoor
  //   -0.5  ~300 ms TET (usually still no digital gain)
  //   -1.5  dim indoor (digital gain kicking in half of the time)
  //   -2.5  night (all shots have digital gain)
  //   -3.5  night
  //   -4.5  dark night
  //   -5.5  really dark night
  //   -6.5  ultra-dark night
  //   -7.5  basically total darkness
  // Note that not all pixels will have the same weight in this computation;
  //   things like CWA, face detection, etc. can boost the importance of
  //   some pixels, and reduce the importance of others.  In general, the
  //   center of the scene, as well as human faces, will have a little more
  //   weight in computing this value.
  float log_scene_brightness;

  // The number of frames in the metering burst.
  int metering_frame_count;

  // The number of frames in the original payload burst capture
  //   (including discarded frames, true long exposure(s), etc).
  int original_payload_frame_count;

  // A vector of the client-provided (i.e. ISP-generated) sharpness values of
  //   the payload frames.
  // The vector should be of length 'original_payload_frame_count'.
  // The values are only meaningful relative to each other; higher means
  //   sharper.  These values should not be compared between different
  //   shots, though.
  std::vector<float> original_payload_frame_sharpness;

  // A vector of the final sharpness values of the payload frames.  If the
  //   client did not provide the sharpness values, then Gcam will produce them,
  //   and the values will be stored here.
  // The vector should be of length 'original_payload_frame_count'.
  // The values are only meaningful relative to each other; higher means
  //   sharper.  These values should not be compared between different
  //   shots, though.
  std::vector<float> final_payload_frame_sharpness;

  // A vector indicating whether each of the payload frames was merged.
  // The vector should be of length 'original_payload_frame_count'.
  std::vector<bool> was_payload_frame_merged;

  // [YUV pipeline only]
  // A vector of how much the contents of each merged frame were globally
  //   translated (on x and y) in order to roughly match the contents of
  //   the base frame.
  // The list should contain (merged_frame_count - 1) entries, since this
  //   information is only relevant for frames that were aligned & merged.
  // The list is *unordered* (it's only meant for compiling statistics).
  std::vector<Point2i> global_pixel_shifts;

  // Was the shot ZSL?
  bool zsl;

  // The zero-based index of the base frame in the payload burst.
  int base_frame_index;

  // The number of payload frames that were merged together, including the
  //   base image.  Range is [1 .. original_payload_frame_count].
  int merged_frame_count;

  // [YUV pipeline only]
  // For non-HDR shots, this equals average_heat / merged_frame_count.
  // For HDR shots, this value is based on that same formula, but is slightly
  //   contaminated; the reason is that this value isn't computed until after
  //   local gains (HDR) are applied, and when we apply those, we scale the
  //   heat values by (1 / X) ^ 2, where X is the local gain (ranging from 1
  //   to actual_range_compression).  (So, for HDR shots, it is possible to
  //   see values here less than 1 / merged_frame_count.)
  // For example, in an HDR shot, if roughly 80% of the pixels come from
  //   the short exposure (no gain), and 20% are locally gained (by a factor
  //   of 'actual_range_compression'), then (roughly) you'd see 20% of the
  //   pixels have their heat value reduced by actual_range_compression ^ 2,
  //   and the other 80% heat values would be unchanged.
  // TODO(geiss): Find a way to compute this (for free) at an earlier stage,
  //   before the heat values are modified (by PHDR), at a place that is
  //   (ideally) common to both HDR- and non-HDR shots, and (ideally) during
  //   the Finish phase (rather than during the time-critical Merge phase).
  float average_heat_frac;

  // These track the two most important elements of capture timing.
  // 'time_to_shot' is the time between the call to StartShotCapture()
  //     and the first call to AddPayloadFrame().
  // 'time_to_postview' is the time between the call to StartShotCapture()
  //     and when the postview is ready for the caller.
  // A value of 0 means the information was not available.
  // Note that these might vary greatly depending on the value of 'zsl'.
  float time_to_shot;       // In seconds.
  float time_to_postview;   // In seconds.

  // Indicates whether Hexagon or IPU were used to process the shot.
  bool used_hexagon;
  bool used_ipu;
};

// Fill in the AE-related fields of ShotLogData from AeResults.
void WriteAeToShotLogData(const AeResults& ae_results,
                          ShotLogData* shot_log_data);

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_SHOT_LOG_DATA_H_
