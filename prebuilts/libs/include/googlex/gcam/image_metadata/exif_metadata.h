#ifndef GOOGLEX_GCAM_IMAGE_METADATA_EXIF_METADATA_H_
#define GOOGLEX_GCAM_IMAGE_METADATA_EXIF_METADATA_H_

#include <cstdint>
#include <string>

#include "googlex/gcam/image/icc_profile.h"
#include "googlex/gcam/image_metadata/client_exif_metadata.h"
#include "googlex/gcam/image_metadata/flash.h"
#include "googlex/gcam/image_metadata/frame_metadata.h"
#include "googlex/gcam/image_metadata/image_rotation.h"
#include "googlex/gcam/image_metadata/static_metadata.h"

namespace gcam {

// Orientations supported by the EXIF orientation tag.
// The description is how the raw data should be transformed to display
// on-screen with correct orientation. Rotations are clockwise.
enum class ExifOrientation {
  kInvalid = 0,     // Invalid or unknown orientation
  kNormal,          // (0,0)-(w,0) is top edge, left to right
  kHorizontalFlip,  // (w,0)-(0,0) is top edge
  kRotate180,       // (w,h)-(0,h) is top edge
  kVerticalFlip,    // (0,h)-(w,h) is top edge
  kTranspose,       // (0,0)-(0,h) is top edge
  kRotate90,        // (0,h)-(0,0) is top edge
  kTranspose180,    // (w,h)-(w,0) is top edge
  kRotate270        // (w,0)-(w,h) is top edge
};

// Minimum digital zoom ratio to encode in EXIF. Otherwise we indicate no
// digital zoom.
const float kMinDigitalZoomRatio = 1.02f;

// Bundle of Gcam image metadata, used when reading/writing JPG or DNG images.
// Both of these file formats encode metadata using a set of TIFF-style tags,
// following the EXIF, TIFF/EP, XMP and DNG specifications.
//
// Not all Gcam metadata fields get written, nor are all Gcam metadata fields
// populated when loading a JPG or DNG file. Helper functions are provided to
// convert between Gcam and EXIF metadata formats.
//
// References:
//   http://www.cipa.jp/std/documents/e/DC-008-2012_E.pdf
//   http://standardsproposals.bsigroup.com/Home/getPDF/567
//   http://www.adobe.com/content/dam/Adobe/en/products/photoshop/pdfs/dng_spec_1.4.0.0.pdf
//   https://wwwimages2.adobe.com/content/dam/Adobe/en/devnet/xmp/pdfs/XMP%20SDK%20Release%20cc-2016-08/XMPSpecificationPart3.pdf
//
struct ExifMetadata {
  ExifMetadata() { SetTimestampCurrent(); }

  StaticMetadata static_metadata;   // Static metadata describing the camera.
  FrameMetadata frame_metadata;     // Metadata for the frame.
  ClientExifMetadata client_exif;   // EXIF metadata directly from the client.

  // Crop rectangle describing what part of the raw image to show by default,
  // relative to the active area. If the crop is empty, the whole raw image is
  // shown; for example, this handles the case where the raw image was
  // pre-cropped before saving it to DNG.
  PixelRect dng_crop;

  // Crop rectangle for the final output, reflected in the final JPG.
  NormalizedRect final_crop;

  // Amount of dynamic range compression suitable for the scene, given as a
  // factor relating the brightness of the scene's shadows and its highlights
  // (in linear terms, before any tone mapping).
  float range_compression = 0.0f;

  // String written to EXIF MakerNote, typically containing debugging
  // information for the shot. The contents will be obfuscated with weak
  // encryption (see jpg_helper_makernote.cc).
  std::string makernote;

  // String appended to StaticMetadata::software when writing the EXIF Software
  // tag. For example, this suffix could encode the capture mode.
  std::string software_suffix;

  // ICC profile specifying the output color space.
  IccProfile icc_profile = IccProfile::kSrgb;

  // White balance mode specified by the app.
  WhiteBalanceMode wb_mode = WhiteBalanceMode::kUnknown;

  // Unix-style timestamp, given as microseconds since January 1, 1970 in UTC.
  // Automatically set from the current time on construction.
  int64_t timestamp_unix_us;

  // Lens shading correction map, corresponding to the active area. If the raw
  // image has already been cropped (i.e. if 'dng_crop' is empty, implying that
  // the full raw image is active), the lens shading correction map should be
  // cropped to match. The map is stored as a low-resolution floating-point
  // image with color channels in canonical [R, Gr, Gb, B] order.
  InterleavedImageF gain_map_rggb;

  // Exposure compensation, above or below auto-exposure, measured in stops.
  //    0 = no bias;
  //   +1 = capture twice as much light as normal;
  //   -1 = capture half as much light as normal;
  //   etc.
  float exposure_compensation = 0.0f;

  // If the device supports flash, this value tells Gcam what mode the flash
  // was in for this shot, from a UI perspective.  You must set it to
  // FlashMode::kAuto (the default), FlashMode::kOn, or FlashMode::kOff.
  // If the device does not have a flash, this must be set to FlashMode::kOff.
  FlashMode flash_mode = FlashMode::kInvalid;

  // How to rotate the raw image for proper on-screen display.
  ImageRotation image_rotation = ImageRotation::kInvalid;

  // Get and set the orientation transform for display.
  // If ShotParams::manually_rotate_final_jpg is true, then the pixels will
  // will be rotated before JPG encoding and ExifMetadata::image_rotation will
  // be reset.
  ExifOrientation Orientation() const;
  void SetOrientation(ExifOrientation exif_orientation);

  // Get and set the flash information, using the 7-bit packed EXIF format.
  // Returns -1 if invalid.
  //
  //       0  : 0 = Flash didn't fire
  //            1 = Flash fired
  //     21   : 00 = No strobe return detection function
  //            01 = Reserved
  //            10 = Strobe return not detected
  //            11 = Strobe return detected
  //   43     : 00 = Unknown
  //            01 = Compulsory flash
  //            10 = Compulsory flash supression
  //            11 = Auto mode
  //  5       : 0 = Flash function present
  //            1 = No flash function
  // 6        : 0 = No red-eye reduction
  //            1 = Red-eye reduction supported
  //
  int Flash() const;
  void SetFlash(int exif_flash);

  // Get and set the sensitivity in ISO units.
  // Note that these functions are *best effort* only. Neither JPG nor DNG
  // files encode enough information to relate ISO to the relative and more
  // detailed way that Gcam stores gains. In particular, these formats have no
  // place to specify the minimum supported gain, nor the breakdown of gain into
  // analog and digital components.
  int Iso() const;
  void SetIso(int iso);

  // Helper functions to compute APEX quantities, derived from frame metadata.
  // https://en.wikipedia.org/wiki/APEX_system.
  double ShutterSpeedValue() const;  // Tv
  double ApertureValue() const;      // Av
  double SensitivityValue() const;   // Sv
  double BrightnessValue() const;    // Bv

  // Get and set the raw noise model in DNG format (three channels, RGB),
  // mapping to the format used by Gcam and Camera2 (four channels, Bayer
  // order). To resolve the ambiguous mapping from Gr/Gb to G we take the
  // noisier of the two channels.
  void NoiseModelRgb(DngNoiseModel* dng_noise_model_rgb) const;
  void SetNoiseModelRgb(const DngNoiseModel dng_noise_model_rgb[]);

  // Set the timestamp from the current time.
  void SetTimestampCurrent();

  // Serialized XMP packets. It's the responsibility of the client to ensure
  // that both XMP strings are valid. Writing XMP data is supported for JPG
  // only.
  //
  // XMP data will be written if 'xmp_metadata_main' is not empty and less than
  // 65502 bytes. Extended XMP data will be written if 'xmp_metadata_extended'
  // is not empty and 'xmp_metadata_main' has a valid "xmpNote:HasExtendedXMP"
  // field with value equal to the MD5 checksum of 'xmp_metadata_extended' as a
  // 32 character hex string with all capital letters. The client is responsible
  // for making sure this checksum is accurate.
  std::string xmp_metadata_main = "";
  std::string xmp_metadata_extended = "";
};

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_METADATA_EXIF_METADATA_H_
