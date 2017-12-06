#ifndef GOOGLEX_GCAM_IMAGE_METADATA_FACE_INFO_H_
#define GOOGLEX_GCAM_IMAGE_METADATA_FACE_INFO_H_

#include "googlex/gcam/base/pixel_rect.h"
#include "googlex/gcam/image_metadata/image_rotation.h"

namespace gcam {

// Each face should be described by a rectangle.
// The ideal guidelines are as follows, but they can be followed
//   approximately:
// First, fit a minimal square such that the top edge passes through
//   the forehead, and the square fully contains both the eyebrows
//   and the mouth.
// If the square has to go a bit beyond the edges of the face (horizontally),
//   because the face is tall/narrow, that is okay.
// Then take the edge length of that square, in pixels, and divide this by
//   the longer dimension of the image (max(width, height)).
// The resulting value (in the [0..1] range) should be used as the 'size'
//   value for the face.
// The pos_x and pos_y values should be the coordinates of the center of
//   this square, where [0,0] is the upper-left corner of the images you're
//   passing in to Gcam (before any Gcam-side digital zoom or crop), and
//   [1,1] is the lower-right corner of the image.
// TODO(geiss): Gcam does not currently use the 'confidence' value.
//
struct FaceInfo {
  float pos_x;       // [0..1] Left..right.
  float pos_y;       // [0..1] Top..bottom.
  float size;        // [0..1] See detailed description above.
  float confidence;  // [0..1], or 0 if confidence information is unavailable.

  bool Equals(const FaceInfo& other) const;
};

// Helper functions for converting between FaceInfo structs and pixel
//   rectangles.  The former are always specified relative to the active pixel
//   area of the original sensor image, in its original orientation.  That image
//   can then be optionally downsampled and/or rotated; the pixel rectangles are
//   then relative to the image after these (optional) transformations.
// Here is an example:
//   (1) original (active area):     3200 x 2400  <- FaceInfo applies here
//   (2) downsampled:                400 x 300
//   (3) rotated (e.g. clockwise):   300 x 400    <- PixelRect applies here
// These two functions let you convert a FaceInfo at stage (1) to a PixelRect
//   at stage (3), and vice versa.
// When passing in the 'w' and 'h' parameters, always pass in the width
//   and height after the downsample but before the rotation, i.e., at stage
//   (2) above.
// NOTE: In practice, calls from libgcam use ImageRotation::kNone, since the
//   image isn't rotated until the very end of the pipeline.  But in ae_test's
//   detail view, we rotate the images at display time; so for proper face
//   rectangles, we must tell these functions about the intended rotation.
PixelRect FaceInfoToPixelRect(
    const FaceInfo& face,
    const int w,
    const int h,
    const bool clamp,
    const ImageRotation future_rotation = ImageRotation::kNone);
FaceInfo PixelRectToFaceInfo(
    const PixelRect& rect_on_rotated_image,
    const float confidence,
    const int w,
    const int h,
    const ImageRotation rotation_already_applied = ImageRotation::kNone);

// Helper function that returns an updated FaceInfo struct to reflect a crop
// applied to the underlying image.
FaceInfo CropFaceInfo(const FaceInfo& face,
                      const PixelRect& crop,
                      int pre_crop_width,
                      int pre_crop_height);

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_METADATA_FACE_INFO_H_
