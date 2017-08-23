#ifndef GOOGLEX_GCAM_IMAGE_METADATA_STATIC_METADATA_H_
#define GOOGLEX_GCAM_IMAGE_METADATA_STATIC_METADATA_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <cstring>
#include <string>
#include <vector>

#include "googlex/gcam/base/log_level.h"
#include "googlex/gcam/base/pixel_rect.h"
#include "googlex/gcam/image_metadata/bayer_pattern.h"

namespace gcam {

class LogSaver;

extern const char* const kStaticMetadataFilename;

// Color calibration metadata, following the DNG spec. DNG files generally
// include color calibration of this form for two different illuminants.
struct ColorCalibration {
  ColorCalibration() { Clear(); }
  void Clear() {
    illuminant = Illuminant::kUnknown;
    static const float kIdentity[] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    memcpy(xyz_to_model_rgb,        kIdentity, sizeof(kIdentity));
    memcpy(model_rgb_to_device_rgb, kIdentity, sizeof(kIdentity));
  }
  bool Equals(const ColorCalibration& other) const {
    return illuminant == other.illuminant &&
           memcmp(xyz_to_model_rgb, other.xyz_to_model_rgb,
                  sizeof(xyz_to_model_rgb)) == 0 &&
           memcmp(model_rgb_to_device_rgb, other.model_rgb_to_device_rgb,
                  sizeof(model_rgb_to_device_rgb)) == 0;
  }

  // The reference illuminants from the EXIF spec.
  enum Illuminant {
    kUnknown = 0,
    kDaylight = 1,
    kFluorescent = 2,
    kTungsten = 3,
    kFlash = 4,
    kFineWeather = 5,
    kCloudyWeather = 10,
    kShade = 11,
    kDaylightFluorescent = 12,
    kDayWhiteFluorescent = 13,
    kCoolWhiteFluorescent = 14,
    kWhiteFluorescent = 15,
    kWarmWhiteFluorescent = 16,
    kStandardLightA = 17,
    kStandardLightB = 18,
    kStandardLightC = 19,
    kD55 = 20,
    kD65 = 21,
    kD75 = 22,
    kD50 = 23,
    kISOStudioTungsten = 24,
    kOther = 255,
  };

  // The illuminant to which this calibration pertains.
  // Defaults to kUnknown.
  Illuminant illuminant;

  // A row-major 3x3 matrix that maps from the XYZ color space to sensor RGB
  //   for this model of camera.
  // Since the primaries have different spectra, the rows will generally not
  //   sum to one. Defaults to identity.
  // For detailed background about the XYZ color space, see:
  //   http://en.wikipedia.org/wiki/CIE_1931_color_space
  float xyz_to_model_rgb[9];

  // A row-major 3x3 matrix that maps from the sensor RGB for this model of
  //   camera (e.g. Nexus 6) to sensor RGB for a specific device (e.g. the unit
  //   with serial number 015DB75A1001900F). This allows for factory calibration
  //   without changing the matrix above.
  // This matrix should be close to the identity and its rows should generally
  //   sum to one. Defaults to identity.
  float model_rgb_to_device_rgb[9];

  // Note: The DNG spec also includes a forward matrix mapping from sensor RGB
  //   to XYZ under the illuminant. If left unset raw converters will compute
  //   this from the above, so we can safely skip this field.
};

// Metadata intrinsic to a given camera (a particular imaging sensor on a
// device), that stays constant over all configurations of the camera, and is
// known before taking any shots. This static metadata does not vary per frame,
// from shot to shot, or between the metering and payload bursts.
//
// While most static metadata is common to all cameras of a particular type
// (e.g. back- or front-facing) for devices of the same make and model, static
// metadata also includes fields that can vary per *individual* camera (e.g.
// the back camera on the unit with serial number 015DB75A1001900F), based on
// per-unit factory calibration. One such example is the field
// ColorCalibration::model_rgb_to_device_rgb.
//
// Static metadata is typically configured once and for all at Gcam pipeline
// creation, for all cameras on a device, based on information provided by the
// device vendor. While static metadata may change slightly over the course of
// device bringup, for a given session using the camera, the static metadata is
// guaranteed to stay fixed.
//
struct StaticMetadata {
  StaticMetadata() { Clear(); }
  void Clear();
  void Print(LogLevel log_level, int indent_spaces) const;
  void SerializeToString(std::string* str, int indent_spaces) const;
  bool DeserializeFromString(const char** str);
  bool Equals(const StaticMetadata& other) const;
  bool Check(bool silent,
             LogSaver* log_saver) const;  // Optional.

  // The manufacturer of the product/hardware (e.g. "Google").
  std::string make;

  // The end-user-visible name for the end product (e.g. "Nexus 5").
  std::string model;

  // The name of the industrial design (e.g. "hammerhead").
  std::string device;

  // Sensor ID, in the range [0, number_sensors-1].
  // Indicates which imaging sensor on the physical device the static metadata
  //   refers to. The distinction is necessary for devices with multiple
  //   cameras, as in a cellphone's front and rear cameras, or an array camera
  //   system.
  // Numeric values aren't meaningful beyond testing for equality -- they
  //   give no indication about properties or arrangement. The sensor ID's used
  //   for static metadata correspond to the sensor ID's reported in per-frame
  //   metadata, FrameMetadata::sensor_id.
  // Defaults to 0.
  int sensor_id;

  // Description of the software used to create the image this metadata
  // describes (e.g. "Gcam [build date: ...], Device code ...").
  std::string software;

  // String that identifies os build number (e.g. taken from
  // android.os.Build.FINGERPRINT)
  std::string device_os_version;

  // Whether the device has a flash unit.
  // If so, all FlashMode settings are available; if not, only FlashMode::kOff
  //   is valid.
  bool has_flash;

  // Range of sensor sensitivities supported by the device.
  // Given as standard ISO sensitivity values, defined in ISO 12232:2006.
  int iso_range[2];

  // Maximum sensor sensitivity implemented purely through analog gain.
  // For values less than or equal to this, all applied gain must be analog.
  // For values above this, the gain can be a mix of analog and digital.
  int max_analog_iso;

  // Dimensions of the full pixel array, possibly including black calibration
  //   pixels that don't receive light from the scene.
  int pixel_array_width;
  int pixel_array_height;

  // The area of the image sensor which corresponds to active pixels.
  // This is the region of the sensor that actually receives light from the
  //   scene. Therefore, the size of this region determines the maximum field of
  //   view and the maximum number of pixels that an image from this sensor can
  //   contain.
  // The rectangle is defined in the coordinates of the full pixel array. In
  //   particular, if the active area covers the full pixel array, the rectangle
  //   will be [0 pixel_array_width 0 pixel_array_height]. Otherwise, the
  //   rectangle will be inside those bounds.
  // TODO(dsharlet): Use this to help determine cropping for the raw pipeline.
  PixelRect active_area;

  // List of disjoint rectangles indicating optically shielded pixels on the
  //   image sensor, providing a reference for black level compensation. These
  //   rectangles must not overlap the active area, and in general only
  //   correspond to a subset of the non-active pixels.
  // The rectangles are defined in the coordinates of the full pixel array.
  std::vector<PixelRect> optically_black_regions;

  // Maximum width and height of the frames, in YUV and raw format.
  // For raw frames, the maximum dimensions may correspond to either the active
  //   pixel array (matching the dimensions specified by 'active_area'), or else
  //   the full pixel array ('pixel_array_width' by 'pixel_array_height'), which
  //   may include black calibration pixels.
  // Sending a frame to Gcam that exceeds these dimensions is considered an
  //   error. These values are used to estimate memory usage, so they should
  //   not be higher than necessary.
  int frame_yuv_max_width;   // For YUV frames.
  int frame_yuv_max_height;
  int frame_raw_max_width;   // For Bayer raw frames.
  int frame_raw_max_height;

  // Number of bits per pixel for a Bayer raw frame, or -1 for unknown.
  // For example, if the device produces RAW10, set this to 10.
  int raw_bits_per_pixel;

  // Color calibrations for different illuminants.
  // Like the DNG spec, we support either one or two color calibrations. Two
  //   color calibrations is more typical, because it gives you something to
  //   interpolate between when adjusting color in post.
  std::vector<ColorCalibration> color_calibration;

  // DEPRECATED: Black levels should be communicated to gcam with
  //   FrameMetadata::black_levels.
  float black_levels_bayer[4];

  // Raw pixel value corresponding to full saturation.
  // This value will often likely be (2^10 - 1) or (2^12 - 1), corresponding
  //   to the maximum representable value for sensor bit depths of 10 and 12
  //   respectively.
  int white_level;

  // Color filter order of the raw Bayer pattern.
  BayerPattern bayer_pattern;

  // The F/number's supported by the lens.
  // In practice, all devices libgcam supports have a fixed aperture. If this
  //   ever changes, we'll need to add F/number to FrameRequest/FrameMetadata.
  std::vector<float> available_f_numbers;

  // The focal lengths, in mm, supported by the lens.
  // In practice, all devices libgcam supports have a fixed focal length, i.e.
  //   no optical zoom. If this ever changes, we'll need to add focal length to
  //   FrameRequest/FrameMetadata.
  std::vector<float> available_focal_lengths_mm;

  // Time required to readout an entire frame, in msec. This is the reciprocal
  // of the maximum frame rate of the camera.
  float frame_readout_time_ms;
};

// Check whether the specified black regions are consistent with the frame
// dimensions and active area given in StaticMetadata, and are also self
// consistent. The field StaticMetadata::optically_black_regions is explicitly
// ignored.
bool CheckBlackRegions(const std::vector<PixelRect>& black_regions,
                       const StaticMetadata& static_metadata);

// Returns a meaningless, randomly generated StaticMetadata.
StaticMetadata GetRandomStaticMetadata();

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_METADATA_STATIC_METADATA_H_
