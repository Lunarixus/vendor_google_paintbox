#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_FRAME_REQUEST_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_FRAME_REQUEST_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include "googlex/gcam/image_metadata/awb_info.h"

namespace gcam {

// Note: We don't necessary need/want to expose this enum, but we can't store
// this state internally b/c the capture can be mocked... so, oh well.
enum BurstFrameType {
  kSingleExp = 0,  // X (deprecated; legacy raw/YUV bursts only)
  kShortExp,       // S
  kLongExp,        // L (deprecated; legacy YUV bursts only)
  kInvalidBurstFrameType
};

// This structure describes a frame that Gcam would like the client to capture.
// It is produced by Gcam and consumed by the client;
//   it is also passed back to Gcam (*unchanged*) once the frame is captured.
// The client should NEVER change the values in a FrameRequest;
//   only Gcam should set or change them.  If the requested values can't be
//   exactly honored, that's OK; leave the values here alone, and set the
//   values *in the FrameMetadata struct* to reflect what was really captured.
// When capturing the frame, the client should try to honor the values here
//   as closely as possible; however, this won't always be possible.
//   For example:
//     -Sometimes an exact exposure time, analog gain, AwbInfo, etc. can't be
//        used, because only certain discrete values are possible.
//     -Client-side flicker reduction might result in significant changes
//        to the exposure time and gain.
// IMPORTANT: The product of (exp_time * analog_gain * digital_gain)
//   for the captured frame (~Metadata) must not exceed that product for the
//   requested frame (~FrameRequest).  To ensure this, if you need to make
//   minor adjustments to any of these 3 values (for what is actually
//   captured), it is safest to round DOWN to the next legal value,
//   rather than up.
// The client can apply the digital gain or not; it is optional.  If the client
//   applies the digital gain, though, it must do so to both the YUV ***and the
//   raw*** frames passed to Gcam (...which likely means that the easiest place
//   to do it is at the sensor, rather than in the ISP).  That way, at the time
//   the information is passed into Gcam, the value encoded here (in the
//   FrameMetadata) is accurate for both the raw and YUV versions of the frame.
//   (This is particularly important for the case where we capture both YUV and
//   raw.)
//
// TODO(hasinoff): Add a new field 'input_tonemap' containing the Tonemap that
//   should be used for capture. In practice, this field will be populated from
//   Tuning::input_tonemap. Adding this field means that a typical client will
//   no longer have to deal with the Tuning struct.
//
struct FrameRequest {
  FrameRequest() { Clear(); }
  void Clear();

  bool Equals(const FrameRequest& other) const;

  // Returns the desired TET (total exposure time), which is just the product
  // of the 3 desired values (exposure time, in ms, analog gain, and digital
  // gain).
  float GetDesiredTet() const;

  // The desired exposure time of the frame, in milliseconds.
  // Do NOT copy this value directly to FrameMetadata::actual_exposure_time_ms;
  //   set that value to what was actually reported as captured, by the HAL.
  //   (Often, there are small differences.)
  // IMPORTANT: If the ISP/sensor/HAL can not capture the frame using the
  //   exact exposure time requested, it should round DOWN to the next legal
  //   value -- not to the nearest.  For example, on Glass v1, for stills, the
  //   ISP/sensor can only use exposure times to that are multiple of the row
  //   time (roughly 33000 us / 1944 rows = 17 us).  Rounding down is preferred
  //   for Gcam because underexposure can be corrected (digitally gained up)
  //   later.  By contrast, overexposure can not be as easily corrected later
  //   (by applying gain of <1x) because white highlights will become gray,
  //   and dynamic range is lost.
  float desired_exposure_time_ms;

  // The desired analog gain.
  // 1.0 means no gain; 3.5 means 3.5x analog gain; and so on.
  // Do NOT copy this value directly to FrameMetadata::actual_analog_gain;
  //   set that value to what was actually reported as captured, by the HAL.
  //   (Often, there are small differences.)
  // IMPORTANT: If the exact analog gain multiplier requested can not be
  //   honored, the ISP/sensor/HAL should round DOWN to the next legal value
  //   -- not to nearest.  (See further explanation in comments under
  //   'desired_exposure_time_ms'.)
  float desired_analog_gain;

  // This is the total amount of digital gain that Gcam wants to be applied
  //   to the frame.  This can be done at the sensor, by the ISP, or by Gcam;
  //   or a mix.
  // 1.0 means no gain; 3.5 means 3.5x digital gain; and so on.
  // If possible, for best results (in terms of image quality and power),
  //   apply this digital gain at the sensor or in the ISP.
  // In that case, set FrameMetadata::applied_digital_gain (for the frame you
  //   produce) to the amount that was actually applied, as reported back
  //   by the HAL (in case there are small differences between what was
  //   requested, and what was applied).
  // Otherwise, if you can't apply any digital gain at the sensor or ISP, set
  //   FrameMetadata::applied_digital_gain to 1.
  // IMPORTANT: If any digital gain is applied by the client, and if the exact
  //   digital gain multiplier requested can not be honored, the ISP/sensor/HAL
  //   should round DOWN to the next legal value -- not to nearest.  (See
  //   further explanation in comments under 'desired_exposure_time_ms'.)
  float desired_digital_gain;

  // The client should try to force the ISP to apply the AWB parameters
  //   specified here.
  // For metering bursts, awb.Check() will return false; in this
  //   case, the client (or HAL) is responsible for determining good WB
  //   settings for each frame.
  // For payload bursts, awb.Check() will return true, and the client/HAL
  //   should use these values to color-process the captured frame.
  // (Whatever WB color processing was actually used on a captured frame
  //   should be reported in FrameMetadata::wb_capture; these values should be
  //   queried from the ISP, though, in case they are different than the
  //   requested values here.)
  AwbInfo awb;

  // Should the sensor try to lock the black level for this frame?
  // When 'true', the sensor should try to lock the black level offset to
  //   whatever it was for the previous sensor frame, when reading out this
  //   frame.
  // When 'false', the sensor will be free to decide for itself whether or
  //   not to recalibrate its black level offset, for this frame.
  // Background: Some sensors have extra masked-off (dark) pixels around the
  //   edges of the image, which are used in dynamic black level measurement
  //   (and subtraction).  However, when Gcam is trying to grab a sequence of
  //   homogenous images (at similar exposure times and gains) (which might
  //   be just a *subset* of a burst), it is best if the frames of this
  //   subset are all produced (at the sensor) with the same black level
  //   subtraction, so that their color is very consistent, and they can
  //   be more reliably merged and/or compared.
  // Additional background:
  //   - In all sensor designs we know of, changing the analog gain
  //       *requires* the dynamic black level offset to be recalibrated.
  //   - Changing exposure (probably) won't trigger a recalibration.
  //   - There are often other triggers, too, like if the mean dark pixels
  //       varies too far outside of the last time it was recalibrated.
  bool try_to_lock_black_level;

  // For internal Gcam tracking; client can ignore.
  BurstFrameType type;
};

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_FRAME_REQUEST_H_
