#ifndef GOOGLEX_GCAM_CREATIVE_CAMERA_PORTRAIT_MODE_PORTRAIT_REQUEST_H_
#define GOOGLEX_GCAM_CREATIVE_CAMERA_PORTRAIT_MODE_PORTRAIT_REQUEST_H_

#include "googlex/gcam/base/pixel_rect.h"
#include "googlex/gcam/image_metadata/image_rotation.h"

namespace creative_camera {

// A list of arguments used by Portrait Mode to finsh processing the image.
struct GoudaRequest {
  // The desired output width and height. The processed images will be upsampled
  // to this resolution.
  // TODO(nealw): Make the above sentence a reality.
  int output_width = 0;
  int output_height = 0;

  // How the image should be transformed to be displayed on-screen with
  // the correct orientation. See googlex/gcam/image_metadata/image_rotation.h.
  gcam::ImageRotation image_rotation = gcam::ImageRotation::kInvalid;

  //  The bounding boxes of faces in the image.
  std::vector<gcam::PixelRect> faces;

  // Metadata related to PD.
  uint16_t pd_black_level = 0;
  uint16_t pd_white_level = 0;
};

}  // namespace creative_camera

#endif  // GOOGLEX_GCAM_CREATIVE_CAMERA_PORTRAIT_MODE_PORTRAIT_REQUEST_H_
