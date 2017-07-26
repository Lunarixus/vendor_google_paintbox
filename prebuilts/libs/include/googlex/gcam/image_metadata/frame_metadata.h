#ifndef GOOGLEX_GCAM_IMAGE_METADATA_FRAME_METADATA_H_
#define GOOGLEX_GCAM_IMAGE_METADATA_FRAME_METADATA_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <cstdint>
#include <string>
#include <vector>

#include "googlex/gcam/base/log_level.h"
#include "googlex/gcam/base/pixel_rect.h"
#include "googlex/gcam/image_metadata/awb_info.h"
#include "googlex/gcam/image_metadata/face_info.h"
#include "googlex/gcam/image_metadata/flash.h"
#include "googlex/gcam/tonemap/tonemap_yuv.h"

namespace gcam {

class LogSaver;

// Constant indicating that the sensor temperature (Celsius) is unknown.
static const int kSensorTempUnknown = -1024;

enum class SceneFlicker {
  kUnknown = 0,
  kNone,
  k50Hz,
  k60Hz
};

const char* ToText(SceneFlicker scene_flicker);
SceneFlicker TextToSceneFlicker(const char* text);

// Lens status.
// https://developer.android.com/reference/android/hardware/camera2/CaptureResult.html#LENS_STATE
enum class LensState {
  kUnknown = -1,
  kStationary = 0,
  kMoving = 1,
};

const char* ToText(LensState lens_state);
LensState TextToLensState(const char* text);

// Current state of the auto-focus (AF) algorithm.
// https://developer.android.com/reference/android/hardware/camera2/CaptureResult.html#CONTROL_AF_STATE
enum class AfState {
  kUnknown = -1,
  kInactive = 0,
  kPassiveScan = 1,
  kPassiveFocused = 2,
  kActiveScan = 3,
  kFocusedLocked = 4,
  kNotFocusedLocked = 5,
  kPassiveUnfocused = 6,
};

const char* ToText(AfState af_state);
AfState TextToAfState(const char* text);

// Description of the noise found in a particular raw/linear image, or one of
// its Bayer color channels, following the DNG specification for the
// 'NoiseProfile' tag.
//
// This model describes noise variance as a linear function of the ideal signal
// level, where the signal is normalized to the range [0, 1]. The model assumes
// the noise is spatially independent (white noise).
//
struct DngNoiseModel {
  // The noise variance for a given signal level y is modeled as:
  //
  //   Var[y] = scale*y + offset
  //
  // where y is the normalized noise-free signal level, given in the range
  // [0, 1], corresponding to the range [black_level, white_level] in the
  // original raw image.
  //
  float scale = 0.0f;
  float offset = 0.0f;

  bool Check() const;
  bool Equals(const DngNoiseModel& other) const;
};

// Metadata for auto-exposure (AE).  This struct is optional and solely used to
// log debugging data. See //googlex/gcam/hdrplus/lib_gcam/shot_params.h
// for how to give vendor-provided AE results to gcam.
// TODO(hasinoff): Create enums mirroring allowable Camera2 values.
struct AeMetadata {
  // The desired mode for the camera device's auto-exposure routine.
  // https://developer.android.com/reference/android/hardware/camera2/CaptureResult.html#CONTROL_AE_MODE
  int mode = -1;

  // Whether AE is currently locked to its latest calculated values.
  // https://developer.android.com/reference/android/hardware/camera2/CaptureResult.html#CONTROL_AE_LOCK
  bool lock = false;

  // Current state of the AE algorithm.
  // https://developer.android.com/reference/android/hardware/camera2/CaptureResult.html#CONTROL_AE_STATE
  int state = -1;

  // Whether the camera device will trigger a precapture metering sequence when
  // it processes this request.
  // https://developer.android.com/reference/android/hardware/camera2/CaptureResult.html#CONTROL_AE_PRECAPTURE_TRIGGER
  int precapture_trigger = -1;

  // Metering rectangles used for auto-exposure. Coordinates correspond to the
  // active region of the sensor with (0,0) in the top-left of the active
  // rectangle. (See CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE in
  // Camera2.)
  // https://developer.android.com/reference/android/hardware/camera2/CaptureResult.html#CONTROL_AE_REGIONS
  std::vector<WeightedPixelRect> metering_rectangles;
};

// Metadata for auto-white balance (AWB). This struct is optional and solely
// used to log debugging data.
// TODO(hasinoff): Create enums mirroring allowable Camera2 values.
struct AwbMetadata {
  // Whether AWB is currently setting the color transform fields, and what its
  // illumination target is.
  // https://developer.android.com/reference/android/hardware/camera2/CaptureResult.html#CONTROL_AWB_MODE
  int mode = -1;

  // Whether AWB is currently locked to its latest calculated values.
  // https://developer.android.com/reference/android/hardware/camera2/CaptureResult.html#CONTROL_AWB_LOCK
  bool lock = false;

  // Current state of AWB algorithm.
  // https://developer.android.com/reference/android/hardware/camera2/CaptureResult.html#CONTROL_AWB_STATE
  int state = -1;

  // Metering rectangles used for auto-white-balance illuminant estimation.
  // Coordinates correspond to the active region of the sensor with (0,0) in the
  // top-left of the active rectangle. (See
  // CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE in Camera2.)
  // https://developer.android.com/reference/android/hardware/camera2/CaptureResult.html#CONTROL_AWB_REGIONS
  std::vector<WeightedPixelRect> metering_rectangles;
};

// Metadata for auto-focus (AF). This struct is optional and solely used to
// log debugging data.
// TODO(hasinoff): Create enums mirroring allowable Camera2 values.
struct AfMetadata {
  // Whether AF is currently enabled, and what mode it is set to.
  // https://developer.android.com/reference/android/hardware/camera2/CaptureResult.html#CONTROL_AF_MODE
  int mode = -1;

  // Current state of AF algorithm.
  AfState state = AfState::kUnknown;

  // Whether the camera device will trigger autofocus for this request.
  // https://developer.android.com/reference/android/hardware/camera2/CaptureResult.html#CONTROL_AF_TRIGGER
  int trigger = -1;

  // Metering rectangles used for auto-focus. Coordinates correspond to the
  // active region of the sensor with (0,0) in the top-left of the active
  // rectangle (See CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE in
  // Camera2.)
  // https://developer.android.com/reference/android/hardware/camera2/CaptureResult.html#CONTROL_AF_REGIONS
  std::vector<WeightedPixelRect> metering_rectangles;
};

// The position of the lens at a certain time.
struct OisPosition {
  // Time in nanoseconds at which the OIS position is recorded. This is on the
  // the same clock as OisMetadata::timestamp_ois_clock_ns.
  int64_t timestamp_ns;

  // Raw register readouts for the X and Y position of the lens. The positive
  // direction is from right to left and bottom to top respectively. The range
  // should be within [-32768, 32767].
  int32_t raw_readout_x;
  int32_t raw_readout_y;

  bool Check() const;
  bool Equals(const OisPosition& other) const;
};

// The maximum number of OisPositions that should be added by the client.
const int kMaxOisPositions = 16;

// Metadata related to optical image stabilization (OIS). This contains
// the lens position at several times during frame capture.
// https://cs.corp.google.com/android/vendor/google/frameworks/camera/experimental2017/java/com/google/android/camera/experimental2017/ExperimentalKeys.java
// See b/62091252 for upcoming API changes.
// TODO(nealw): Update this struct when this API is updated during the 2017 MR1
//   release.
struct OisMetadata {
  // Time in nanoseconds at which the first row of the frame is exposed as
  // recorded by the CPU clock. All OIS timestamps are on the same clock.
  // The zero point is arbitrary and not necessarily consistent with the Camera2
  // frame timestamp recorded in FrameMetadata::timestamp_ns.

  int64_t timestamp_ois_clock_ns = 0;

  // A multiplicative factor to convert the values in OisPosition::raw_readout_x
  // and OisPosition::raw_readout_y to pixels. In pixel units, the positive
  // direction corresponds to going from left to right and top to bottom.
  float raw_to_pixels = -1.0f;

  // OIS position data of where the lens was at several times during frame
  // capture. The client should not add more than kMaxOisPositions values to
  // this vector. This limit is not enforced in gcam.
  std::vector<OisPosition> ois_positions;

  bool Check() const;
  bool Equals(const OisMetadata& other) const;
};

// This structure contains metadata for an actual frame captured by the HAL
//   and then passed to Gcam.  It is usually produced by the client and
//   passed into Gcam.  (This is usually in response to a FrameRequest
//   struct, which describes a frame that Gcam wants the HAL to capture,
//   and which is produced by Gcam and consumed by the client.)
// In general, be very careful to populate this struct with the actual
//   values used -- do not copy any values from the FrameRequest struct.
// TODO(geiss): Remove all fields that don't belong in the public API, using
//   a wrapper struct like this:
//     struct FrameMetadataPlus {
//       const FrameMetadata orig;   // Original, pristine metadata from client.
//       FrameMetadata cur;          // Current/adjusted metadata.
//
//       // gcam-private temporary storage:
//       float sharpness;
//       float desired_overall_digital_gain;
//     };
struct FrameMetadata {
  FrameMetadata() { Clear(); }
  void Clear();
  void Print(LogLevel log_level, int indent_spaces) const;
  void SerializeToString(std::string* str, int indent_spaces) const;
  bool DeserializeFromString(const char** str, int version);
  bool Equals(const FrameMetadata& other) const;

  // Returns the current TET of the image.  TET is the product of
  //   real exposure time, analog gain, and digital gain applied *so far*,
  //   so this value will vary during processing.  By the end of processing,
  //   all digital gain should have been applied, and the value returned
  //   here should be the same as that returned by GetFinalDesiredTet().
  float GetCurrentTet() const;

  // Returns the final desired TET of the image, after all digital gain
  //   has been applied.
  float GetFinalDesiredTet() const;

  // Once the client passes a frame (with FrameMetadata) into Gcam, Gcam
  //   calls this function to sanity-check all fields of the FrameMetadata
  //   struct, and print warnings/errors.  (Clients can also call it to
  //   check their own values -- just pass nullptr for the 'save' parameter.)
  // Set 'silent' to true if you want the return value only, with no
  //   log output.
  // A 'true' return value means success (no warnings or errors).
  bool Check(const char* frame_type,
             int frame_index,
             bool silent,
             LogSaver* log_saver) const;  // Optional.

  // IMPORTANT: If you add new members, be sure to not only update the
  //   Check and Equals functions here, but also, the SerializeToString
  //   and DeserializeFromString functions.

  // The real exposure time (in milliseconds) of the image.
  // Be sure to set this to match what was ACTUALLY done by the sensor
  //   -- in case it is different from the value that Gcam requested
  //   (in FrameRequest).
  // If this frame was generated by summing together multiple consecutive
  //   frames, i.e. if 'temporal_binning_factor' is greater than 1, this field
  //   corresponds to the sum of all exposure times for the constituent set of
  //   summed frames.
  float actual_exposure_time_ms;

  // The analog gain applied to the image at capture time.
  // Usually [1.0 .. 16.0].
  // Be sure to set this to match what was ACTUALLY done by the sensor
  //   -- in case it is different from the value that Gcam requested
  //   (in FrameRequest).
  // Must be >= 1.
  float actual_analog_gain;

  // The total digital gain that has already been applied to the frame, not
  //   including post-raw digital gain applied by the ISP. (We currently do not
  //   support YUV input frames that have such post-raw gain applied.)
  // Be sure to set this to match what was ACTUALLY done by the ISP / sensor
  //   -- in case it is different from the value that Gcam requested
  //   (in FrameRequest).
  // The client should set this to a value > 1.0 (corresponding to the digital
  //   gain applied) if digital gain is applied at the sensor or ISP, or to 1.0
  //   otherwise (in which case, Gcam will apply the digital gain in
  //   software).
  // Internally, during processing, Gcam will use this value to keep track
  //   of how much digital gain is left to be applied; by the end of Gcam's
  //   processing, this value should be equal to 'desired_digital_gain'.
  // Must be >= 1.
  float applied_digital_gain;

  // Additional post-raw digital gain, applied by the ISP *after* the raw frame
  //   is captured.
  // Post-raw digital gain only affects the brightness of the ISP-processed YUV
  //   frames and any downstream results. It effectively describes how much the
  //   raw input was underexposed relative to the viewfinder.
  // Must be >= 1.
  float post_raw_digital_gain;

  // The client can completely ignore this value; it is set and managed by
  //   Gcam internally.
  // The total digital gain that ultimately should be applied to the frame,
  //   before processing is complete.  1.0 or greater.
  // NOTE: This field is ignored by the Equals() function.
  // NOTE: The value in this field might be modified by Gcam.
  float desired_overall_digital_gain;

  inline float AppliedOverallGain() const {
    return actual_analog_gain * applied_digital_gain;
  }

  // For Gcam internal use only.
  inline float DesiredOverallGain() const {
    return actual_analog_gain * desired_overall_digital_gain;
  }

  // Number of consecutive (in time) frames that have been summed together to
  // generate this frame. This field is typically not touched by the client.
  int temporal_binning_factor;

  // Use this to echo back the mode that the LED flash was in during the capture
  // of the frame.
  FlashMetadata flash;

  // White balance information, consisting of WB gains and CCM's.
  // For the gains, the channel order is [R, Gr, Gb, B].
  // The CCM's map from sensor RGB to sRGB, as in Camera2, regardless of the
  //   ultimate output color space.
  // There are two sets of WB information:
  //   - One set for the actual WB applied to the image that this metadata
  //     represents.
  //   - One set representing the ideal WB that would have been applied.
  // Most of the time, these are one and the same. For legacy systems (e.g.
  //   Glass), there is a distinction. When capturing a burst on those systems,
  //   "older" WB is applied to a frame at capture time.  Meanwhile, in
  //   parallel, fresh AWB is computed from the [raw] frame.  Both of these
  //   results come in at around the same time: the finished frame (but with
  //   potentially 'off' WB gains), and the 'ideal' WB gains that would have
  //   been used (had they magically been known earlier).  During gcam's
  //   processing, we can convert the image back to linear and apply the
  //   difference, so that we end up with the ideal WB.
  AwbInfo wb_capture;  // Corresponds to the actual captured image.
  AwbInfo wb_ideal;    // 'Better' information, computed post-capture.

  // Estimated neutral color point in native sensor RGB color space. Typically
  //   produced by an AWB algorithm. Defaults to (1, 1, 1). Scale is ignored,
  //   so (1, 1, 1) is equivalent to (100, 100, 100).
  // In practice, this is generally equal to the component-wise reciprocal of
  //   the gains reported in wb_capture.gains or wb_ideal.gains, after averaging
  //   the Gr and Gb channels.
  // Ignored entirely if you custom white-balance a DNG in post.
  float neutral_point[3];

  // Some measure of the sharpness of the image, if available; if not,
  //   set it to 0 (and Gcam will spend extra resources computing it).
  // The scale does not matter; the only requirement is that, for
  //   images with the same capture settings (exposure time, analog gain,
  //   digital gain, and WB gains), a sharper image should result in a
  //   higher number.
  // NOTE: The value in this field might be modified by Gcam.
  float sharpness;  // Higher = sharper; 0 if unknown.

  // Sensor temperature (Celsius), if known; otherwise, set to
  //   kSensorTempUnknown.
  int sensor_temp;

  // The time at which the exposure for the first row of the image began,
  //   in nanoseconds.
  // Note that this corresponds to the START of exposure, not the end.
  // (This follows the timestamp definition in the Android camera HAL 3, which
  //   is synchronized with the system clock.)
  // The timestamps should come from a monotonic clock.  The zero point
  //   is arbitrary; Gcam only cares about the relative differences between
  //   timestamps.
  int64_t timestamp_ns;   // Start-of-exposure, in nanoseconds.

  // The tonemapping curve applied to the frame, or as close as possible to the
  //   nonlinear/nonglobal transforms actually used. For raw images, this curve
  //   is what would have been applied if the frame were processed.
  // This field must be filled out if a YUV image is included in the frame the
  //   metadata refers to, in order to describe how the YUV image was
  //   tonemapped.  If only a raw image is provided, filling out the field is
  //   optional.
  Tonemap tonemap;

  // Indicates whether or not the sensor black level offset was force-locked
  //   for this frame (to whatever it was for the previous sensor frame).
  // If the sensor black level was specifically locked for this frame -
  //   meaning that the black level offset applied on-sensor, to this frame,
  //   was forced to be the same offset used on the previous frame - then set
  //   this to true.
  // Otherwise, the sensor was free to decide for itself whether or not to
  //   recalibrate its black level offset, so this should be set to false.
  //   (In that case, the black level offset might have changed, or might
  //   have stayed the same; it's not known.)
  bool was_black_level_locked;

  // Information about detected faces.
  std::vector<FaceInfo> faces;

  // If anything went wrong when capturing the frame, classify each issue
  //   as a warning or an error, and add it to the list of captures warnings
  //   or errors for the frame.
  // Each string you add should be a single line of text ('\r' and '\n' are
  //   not allowed), but the length can exceed 80 characters.
  // If the capture went OK, leave these as empty vectors.
  // If either vector is nonempty, the warnings/errors will be recorded in
  //   Gcam's logcat, and in its executive summary.
  // Note that a nonempty "capture_errors" vector will cause Gcam to abort
  //   the shot capture (fail hard).
  std::vector<std::string> capture_warnings;
  std::vector<std::string> capture_errors;

  // Sensor ID, in the range [0, number_sensors-1].
  // Indicates which imaging sensor on the physical device the frame is from.
  //   The distinction is necessary for devices with multiple cameras, as in a
  //   cellphone's front and rear cameras, or an array camera system.
  // Numeric values aren't meaningful beyond testing for equality -- they
  //   give no indication about properties or arrangement. The sensor ID's used
  //   for per-frame metadata correspond to the sensor ID's reported in static
  //   metadata, StaticMetadata::sensor_id.
  // Defaults to 0.
  int sensor_id;

  // Whether the scene appears to be flickering, and its estimated frequency.
  //   This information is useful for autoexposure, where appropriate choice of
  //   exposure time can help minimize banding artifacts.
  // Defaults to kFlickerUnknown (flicker detection result unavailable).
  SceneFlicker scene_flicker;

  // Noise model for each Bayer color channel of the raw image.
  // The values are the raw noise models for the top-left 2x2 pixels of the
  //   sensor, in row-column scan order (or "reading" order): upper-left,
  //   upper-right, lower-left, and lower-right.
  // In practice, the same noise model often applies to all color channels,
  //   because the sensor elements behind the Bayer color filter array are
  //   generally identical.
  // NOTE: The value in this field might be modified by Gcam.
  DngNoiseModel dng_noise_model_bayer[4];

  // Fixed black level offsets for the 4 color channels of the color filter
  //   array (CFA) mosaic channels.
  // Also sometimes called the 'data pedestal' values.
  // The values are the black level offsets for the top-left 2x2 pixels of
  //   the sensor, in row-column scan order (or "reading" order): upper-left,
  //   upper-right, lower-left, and lower-right.
  // These only need to be set when Bayer raw images are provided to Gcam;
  //   for YUV images, the black level is assumed to be 0. For raw images, these
  //   black levels should always be set, even if the black levels are constant
  //   for all frames. Despite this requirement, these black levels may be
  //   replaced if the frames contain optically black pixels (specified in
  //   StaticMetadata::optically_black_regions).
  // If no Bayer raw image was captured, or if the Bayer raw black level
  //   offsets are otherwise unknown, then set these four values to -1,
  //   which indicates that the Bayer raw black level offsets are unknown.
  // A common value for, say, 10-bit Bayer data, might be around 64.
  float black_levels_bayer[4];

  // Distance to plane of sharpest focus, in units of diopters, measured from
  //   the frontmost surface of the lens.
  // Zero for fixed-focus lenses (b/30207877).
  // Defaults to -1 (unknown).
  float focus_distance_diopters;

  // --------------------------------------------------------------------------
  // 3A and lens state metadata from Camera2.
  //   None of this metadata is surfaced in user-visible EXIF tags. The only
  //   effect on processing is to help base frame selection. Mostly these fields
  //   are just saved for debugging.
  // For more explanation of specific fields see:
  //   https://developer.android.com/reference/android/hardware/camera2/CaptureResult.html
  //   //NOLINT

  // TODO(hasinoff): Create enums mirroring allowable Camera2 values.

  // Overall mode of 3A (auto-exposure, auto-white-balance, auto-focus) control
  // routines.
  // https://developer.android.com/reference/android/hardware/camera2/CaptureResult.html#CONTROL_MODE
  int control_mode;

  // Metadata for client-provided 3A routines.
  AeMetadata  ae;
  AwbMetadata awb;
  AfMetadata  af;

  // Current lens status.
  LensState lens_state;

  // --------------------------------------------------------------------------
  // Vendor-specific metadata plumbed through Camera2
  // experimental tags.
  OisMetadata ois_metadata;
};

// Write the metadata for a burst of captured frames to a string.
void SerializeBurstMetadata(const std::vector<FrameMetadata>& burst_metadata,
                            std::string* str, int indent_spaces);

// Read the metadata for a burst of captured frames from a string.
// If 'burst_metadata' is an empty vector, it will be built up from scratch,
//   from the burst metadata in the string.
// Otherwise, the length of the vector must match the number of frames in the
//   burst, and the bits of burst metadata extracted from the string will be
//   layered on top of the existing contents.
// The optional 'legacy_tonemap' parameter indicates whether the metadata is old
//   enough that the tonemapping curve is expected to be invalid.
bool DeserializeBurstMetadata(const char** str,
                              std::vector<FrameMetadata>* burst_metadata,
                              bool* legacy_tonemap);  // Output, optional.

// Log the color temperature and white balance gains, both captured and ideal,
//   for the given frames.
void PrintColorTemps(const std::vector<FrameMetadata>& burst);

FrameMetadata GetRandomFrameMetadata();

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_METADATA_FRAME_METADATA_H_
