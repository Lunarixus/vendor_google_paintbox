#ifndef GOOGLEX_GCAM_CREATIVE_CAMERA_PORTRAIT_MODE_PORTRAIT_REQUEST_H_
#define GOOGLEX_GCAM_CREATIVE_CAMERA_PORTRAIT_MODE_PORTRAIT_REQUEST_H_

#include "googlex/gcam/base/pixel_rect.h"
#include "googlex/gcam/image_metadata/frame_metadata.h"
#include "googlex/gcam/image_metadata/image_rotation.h"
#include "googlex/gcam/image_metadata/static_metadata.h"

namespace creative_camera {

// How PD was processed prior to portrait mode. This should be updated any
// time there is a change in PD align and merge in HDR+ that would affect
// portrait reprocessing.
// Version 0: Original.
// Version 1: Fixed a bug in version 0, in which the PD data was shifted
//            relative to the RGB image by a few pixels in x and y that depends
//            on the crop.
static const int kPdCurrentVersion = 1;

// LINT.IfChange
// A list of arguments used by Portrait Mode to finsh processing the image.
struct GoudaRequest {
  // Unique id of the request, required to associate all the callbacks with
  // the request. Guaranteed to be unique per entry into the portrait mode.
  int64_t id;

  // The desired output width and height. The processed images will be resampled
  // to this resolution. 'output_width' and 'output_height' must positive.
  int output_width = 0;
  int output_height = 0;

  // Amount to sharpen images after resampling to the requested resolution.
  float post_resample_sharpening = 0.0f;

  // How the image should be transformed to be displayed on-screen with
  // the correct orientation. See googlex/gcam/image_metadata/image_rotation.h.
  gcam::ImageRotation image_rotation = gcam::ImageRotation::kInvalid;

  //  The bounding boxes of faces in the image.
  std::vector<gcam::PixelRect> faces;

  // Merged frame and static metadata from HDR+.
  gcam::FrameMetadata frame_metadata;
  gcam::StaticMetadata static_metadata;

  // Version number corresponding to how PD was processed prior to portrait
  // mode. This field should not be set manually.
  int pd_version = kPdCurrentVersion;

  // A path at which to save the raw inputs to the GoudaProcessor. To skip
  // saving raw inputs, leave this string empty. This field is not serialized.
  std::string portrait_raw_path;

  // TODO(nealw): Remove this field and encode per-shot information in the
  //   portrait_raw_path.
  // The name of a subfolder to save portrait inputs and artifacts into.
  // Ideally, this should somehow be tied to the result images that come out of
  // portrait mode, but anything unique would be fine. If left empty, a
  // timestamp will be generated. This field is not serialized.
  std::string shot_prefix;
};
// LINT.ThenChange(//depot/google3/googlex/gcam/creative_camera/portrait_mode/\
//                 portrait_request_serialization.cc)
}  // namespace creative_camera

#endif  // GOOGLEX_GCAM_CREATIVE_CAMERA_PORTRAIT_MODE_PORTRAIT_REQUEST_H_
