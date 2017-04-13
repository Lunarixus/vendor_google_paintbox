#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_SHOT_PARAMS_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_SHOT_PARAMS_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <string>
#include <vector>

#include "googlex/gcam/ae/ae_shot_params.h"
#include "googlex/gcam/base/log_level.h"
#include "googlex/gcam/base/pixel_rect.h"
#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/init_params.h"
#include "googlex/gcam/image_metadata/awb_info.h"
#include "googlex/gcam/image_metadata/flash.h"
#include "googlex/gcam/image_metadata/image_rotation.h"
#include "googlex/gcam/image_metadata/static_metadata.h"

namespace gcam {

extern const char* const kShotParamsFilename;

static const int kDefaultFullMeteringSweepFrameCount = 6;
static const int kDebugFullMeteringSweepFrameCount = 9;

static const float kDefaultFrameReadoutTimeMs = 33.33f;

// Parameters for each 'shot' (from the user's point of view).
// On construction, this contains a good set of defaults you can use.
//
// See also:
//   ShotParams GetRandomShotParams()
//
struct ShotParams {
  ShotParams() { Clear(); }
  void Clear();  // Applies default settings.
  // Return whether the ShotParams pass a basic validity check.
  // The arguments are optional, but must be provided in order to check the
  //   validity of the 'full_metering_sweep_frame_count' and 'flash_mode' fields
  //   respectively.
  bool Check(const InitParams* init_params,
             const StaticMetadata* static_metadata) const;
  void Print(LogLevel log_level) const;
  void SerializeToString(std::string* str) const;
  // Initialize from the string (which is presumed to come from a previous
  // call to SerializeToString).  On failure, returns false and leaves the
  // ShotParams in a partially-initialized state.
  bool DeserializeFromString(const std::string& str);
  bool Equals(const ShotParams& other) const;

  // This nested struct contains just the parameters that are needed to perform
  // AE on a single viewfinder frame, when running Smart Metering *and*
  // running AE (in a background thread) on the viewfinder frames.
  AeShotParams ae;

  // The number of metering frames to capture, *if* we have no prior
  //   information about the scene's brightness, and decide to capture
  //   a full sweep (from super-bright to super-dark).
  int full_metering_sweep_frame_count;

  // If true, and if previous_viewfinder_tet > 0, then this option will
  //   reduce the number of metering frames captured, resulting in a
  //   faster time-to-shot.
  // This only applies when an explicit metering burst is captured; it does not
  //   apply when Smart Metering is successfully used.  (But if Smart Metering
  //   is not 'forced', and fails because too many pixels are clipped, then an
  //   explicit metering burst will be captured, and this does apply.)
  // When Gcam is used to take a shot from a cold camera (no viewfinder),
  //   then previous_viewfinder_tet is 0, and we capture a full HDR sweep
  //   of exposures (from super-short to super-long), of exactly
  //   'full_metering_sweep_frame_count' metering frames.
  // But when Gcam is used after viewfinding, and previous_viewfinder_tet
  //   is > 0, we can narrow the search and skip some metering frames
  //   that are far away from the viewfinder TET.  In this case, we use
  //   just 3 metering frames.
  bool cull_metering_frames_using_previous_viewfinder;

  // Longest exposure time *preferred* for metering.
  // You probably want to set this to 33.3333 ms if capturing metering frames
  //   at 30 Hz, or to 16.6667 ms if capturing metering frames at 60 Hz.
  float metering_preferred_max_exposure_time_ms;

  // Extra gain factor for metering.  Normally 1, but if your metering
  // frames are 2x or 4x as bright as the equivalent payload frames
  // (because you are using sensor binning and running at 60 Hz, for example),
  // use 2 or 4 here.
  float metering_frame_brightness_boost;

  // If force_wb is provided (and is fully valid - i.e. force_wb.Check()
  //   returns true), then all Gcam frames (metering & payload) will be forced
  //   to capture with this white balance, and all incoming frames (metering &
  //   payload) will have their "wb_ideal" overwritten with this, as well.
  //   As a result, the final shot will have *exactly* this white balance.
  AwbInfo force_wb;                   // Optional.

  // If the device supports flash, this value tells Gcam what mode the flash
  //   was in for this shot, from a UI perspective.  You must set it to
  //   FlashMode::kAuto (the default), FlashMode::kOn, or FlashMode::kOff.
  // If the device does not have a flash, this must be set to FlashMode::kOff.
  FlashMode flash_mode;

  // Exposure level and/or white balance information (from the viewfinder)
  //   should be placed here, if available and converged.
  // If they have not converged, then it is best to omit them.
  // If Gcam advised you to turn the flash on for this shot, and you turned
  //   the flash on and waited for 3A to re-converge, then these two values
  //   (_tet and _wb) should reflect the state of the viewfinder 3A at the
  //   end of that period.  Otherwise, they should reflect the state of the
  //   viewfinder 3A when the shutter button was pressed.
  // Note that if (flash_mode == kFlashAuto), then previous_viewfinder_tet is
  //   currently required.  (We could fix this, but time-to-shot would
  //   increase, it would add a lot of complexity, and we currently don't
  //   anticipate this use case.)
  // Do not provide this information if it was unstable or of poor quality.
  // The "tet" (total exposure time) value should be set to the product of the
  //   exposure time (in milliseconds), analog gain (>= 1.0), and digital gain
  //   (>= 1.0).
  // Gcam will use the information in various ways to improve its output.
  //   For example, it will to try to make sure the exposure level it
  //   chooses isn't *too* different from what the viewfinder was showing.
  //   It might also use parts of 'previous_viewfinder_wb' as a starting point
  //   for the metering frames.  However, in general, the exact details of
  //   when and how this information is used are up to Gcam.
  float   previous_viewfinder_tet;  // Required if (flash_mode == kFlashAuto).
  AwbInfo previous_viewfinder_wb;   // Optional.

  // How to rotate the raw image for proper on-screen display.  This image
  //   rotation applies to debugging images written to disk, and also
  //   determines the EXIF rotation tag saved in the final JPG.
  // The uncompressed final image returned (programatically) by Gcam is not
  //   rotated, unless 'manually_rotate_final_image' is true.  The raw input
  //   images are never rotated when saved to disk.
  ImageRotation image_rotation;

  // Whether Gcam should manually rotate the content of the final image to be
  //   in the proper orientation, as specified in the 'image_rotation'
  //   parameter.  (The default value is false.)
  // This applies even if you are not writing a JPG, or using
  //   ClientExifMetadata. If the image returned is a jpeg-blob-in-memory, then
  //   the pixels will actually be rotated before encoding, and the EXIF
  //   orientation tag, if specified, will be reset.
  // Manually rotating the image incurs a performance penalty, and it should
  //   be avoided if possible.
  // NOTE: *** This flag does not affect the postview image. ***
  bool manually_rotate_final_image;

  // Whether Gcam should manually rotate the content of the postview image to
  //   be in the proper orientation as specified in the 'image_rotation'
  //   parameter.  (The default value is false.)
  // Manually rotating the image incurs a performance penalty, and it should
  //   be avoided if possible, particularly since time-to-postview affects the
  //   user experience of photo-taking latency.
  // NOTE: *** This flag does not affect the final image. ***
  bool manually_rotate_postview_image;

  // If base_frame_override_index is non-negative, then Gcam will override the
  //   selection of the base frame index with this value.  This will lead to an
  //   error if the index is out of range, if the specified frame was dropped,
  //   or if the specified frame is not of the required type (e.g., for an HDR
  //   burst the base frame must be of type kShortExp).
  int base_frame_override_index;

  // Whether to encode the merged raw image to DNG and push the encoded blob
  //   through InitParam::merged_dng_callback. This setting is only relevant if
  //   the callback is defined. It's independent of saving the merged DNG to
  //   disk for debugging via GCAM_SAVE_MERGED.
  // Default: false.
  bool save_merged_dng;

  // Whether to use compression when encoding the merged DNG.
  // The compression method used is lossless JPEG 1992 (LJ92), the oldest
  //   supported compression method, and the one configured by the DNG SDK. It
  //   compresses 10-bit 12MP images from 24mb down to about 10mb, at the
  //   expense of about 60% longer encoding time.
  // Although LJ92-compressed DNG's are supported by most software that read
  //   DNG's, there are annoying compatability problems (e.g. Adobe Lightroom
  //   for Android).
  // Default: false.
  bool compress_merged_dng;

  // Quality setting for the JPG encoder (range 0-100), for the final jpg
  //   result.  Higher quality settings correspond to larger file sizes with
  //   better image quality.
  // Default: 95.
  // This default corresponds to android.media.CameraProfile.QUALITY_HIGH on
  //   many Android devices.
  int final_jpg_quality;

  // Whether to generate a thumbnail for the final JPG, if one was not already
  //   passed in via ClientExifMetadata::thumbnail.
  // Default: true.
  bool generate_jpg_thumbnail;

  // Whether the shot provided to Gcam is zero shutter lag (ZSL).
  // NOTE: ZSL shots only support raw frames; not YUV.
  // ZSL shots are processed slightly differently:
  //   1. The logic about how the base frame is selected is different; it uses
  //        zsl_base_frame_index_hint.
  //   2. Begin/Add/EndMeteringFrames calls are illegal for a ZSL shot.
  //   3. AE will be run on the base frame, and the brightness (and HDR shadow
  //        boost) of the shot will be based on that.
  // TODO(dsharlet): This feature is not yet complete; alignment needs
  //   modifications to properly support aligning frames at different exposure
  //   levels.
  bool zsl;

  // For ZSL shots, the client can pass a hint to Gcam about what it thinks is
  //   a good base frame index here, or -1 for no hint.
  // If -1, then Gcam will pick from several frames with the largest (most
  //   recent) timestamps.
  // Ignored for non-ZSL shots.
  int zsl_base_frame_index_hint;

  // String appended to StaticMetadata::software when writing the EXIF Software
  //   tag. For example, this suffix could encode the capture mode.
  // Default: empty string.
  std::string software_suffix;

  // White balance mode specified by the app.
  WhiteBalanceMode wb_mode;
};

// Generate random ShotParams.
// If InitParams are provided, the returned ShotParams will be consistent with
//   those parameters.
// This function is used by both Brutalizer, and by unit tests that verify that
//   random ShotParams (a) can be saved and re-loaded (b) and still pass the
//   "a.Equals(b)" test.
ShotParams GetRandomShotParams(InitParams* init_params);

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_SHOT_PARAMS_H_
