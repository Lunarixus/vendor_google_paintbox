// This file defines objects and functions to be exposed to Java via SWIG. It is
// meant to be a thin wrapper around the PortraitProcessor module that exposes
// only the app-side required methods.
#ifndef GOOGLEX_GCAM_CREATIVE_CAMERA_PORTRAIT_MODE_PORTRAIT_SWIG_INTERFACE_H_
#define GOOGLEX_GCAM_CREATIVE_CAMERA_PORTRAIT_MODE_PORTRAIT_SWIG_INTERFACE_H_

#include <memory>
#include <vector>

#include "googlex/gcam/base/pixel_rect.h"
#include "googlex/gcam/image/t_image.h"
#include "googlex/gcam/image/yuv.h"
#include "googlex/gcam/image_metadata/image_rotation.h"

namespace creative_camera {

class PortraitProcessor;

class GoudaSwigWrapper {
 public:
  GoudaSwigWrapper() : processor_(nullptr) {}
  ~GoudaSwigWrapper() { Release(); }

  // Initializes the internal PortraitProcessor. This must be called before
  // Process(). Returns true on success.
  bool Init(const string& model_path);

  // Processes a portrait image, returning a background defocused result.
  // rgb_output should be an image of the same size as rgb_input. Returns true
  // on success.
  bool Process(const gcam::InterleavedImageU8& rgb_input,
               gcam::ImageRotation input_rotation,
               const std::vector<gcam::PixelRect>& faces,
               const gcam::InterleavedReadViewU16& pd_input,  // Optional.
               const gcam::InterleavedWriteViewU8& rgb_output);

  bool ProcessRgb(const gcam::InterleavedImageU8& rgb_input,
                  gcam::ImageRotation input_rotation,
                  const std::vector<gcam::PixelRect>& faces,
                  const gcam::InterleavedReadViewU16& pd_input,  // Optional.
                  const gcam::InterleavedWriteViewU8& rgb_output);

  bool ProcessRgba(const gcam::InterleavedImageU8& rgba_input,
                   gcam::ImageRotation input_rotation,
                   const std::vector<gcam::PixelRect>& faces,
                   const gcam::InterleavedReadViewU16& pd_input,  // Optional.
                   const gcam::InterleavedWriteViewU8& rgba_output);

  bool ProcessYuv(const gcam::YuvReadView& yuv_input,
                  gcam::ImageRotation input_rotation,
                  const std::vector<gcam::PixelRect>& faces,
                  const gcam::InterleavedReadViewU16& pd_input,  // Optional.
                  const gcam::YuvWriteView& yuv_output);

  // Releases memory associated with the internal PortraitProcessor. Will
  // require that Init() be called again before any further Process() calls.
  void Release();

 private:
  PortraitProcessor* processor_;
};

}  // namespace creative_camera

#endif  // GOOGLEX_GCAM_CREATIVE_CAMERA_PORTRAIT_MODE_PORTRAIT_SWIG_INTERFACE_H_
