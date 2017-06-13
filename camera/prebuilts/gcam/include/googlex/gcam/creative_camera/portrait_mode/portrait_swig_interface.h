// This file defines objects and functions to be exposed to Java via SWIG. It is
// meant to be a thin wrapper around the GoudaProcessor module that exposes only
// the app-side required methods.
#ifndef GOOGLEX_GCAM_CREATIVE_CAMERA_PORTRAIT_MODE_PORTRAIT_SWIG_INTERFACE_H_
#define GOOGLEX_GCAM_CREATIVE_CAMERA_PORTRAIT_MODE_PORTRAIT_SWIG_INTERFACE_H_

#include <memory>
#include <vector>

#include "googlex/gcam/base/pixel_rect.h"
#include "googlex/gcam/creative_camera/portrait_mode/portrait_callbacks.h"
#include "googlex/gcam/image/t_image.h"
#include "googlex/gcam/image/yuv.h"
#include "googlex/gcam/image_metadata/image_rotation.h"

namespace creative_camera {

class GoudaProcessor;

class GoudaSwigWrapper {
 public:
  GoudaSwigWrapper() : processor_(nullptr) {}
  ~GoudaSwigWrapper() { Release(); }

  // Initializes the internal GoudaProcessor. This must be called before
  // Process(). Returns true on success.
  bool Init();

  // Processes a portrait image, returning a background defocused result.
  //
  // If 'pd_input' is not null, uses PD data to assist in computing the
  // background defocused result.
  //
  // 'id' should be a unique identifier for this portrait. It should not be
  // duplicated for the lifetime of this GoudaProcessor.
  //
  // 'callbacks' is a collection of callback pointers.
  // - 'callbacks.image_callback' must not be nullptr.
  // - For each callback pointer that is not nullptr, its lifetime must persist
  //   until this call to Process() returns.
  //
  // Returns true on success.
  bool Process(int64_t id, const GoudaCallbacks& callbacks,
               const gcam::InterleavedImageU8& rgb_input,
               gcam::ImageRotation input_rotation,
               const std::vector<gcam::PixelRect>& faces,
               const gcam::InterleavedReadViewU16& pd_input);  // Optional.

  // Releases memory associated with the internal GoudaProcessor. Will require
  // that Init() be called again before any further Process() calls.
  void Release();

 private:
  GoudaProcessor* processor_;
};

}  // namespace creative_camera

#endif  // GOOGLEX_GCAM_CREATIVE_CAMERA_PORTRAIT_MODE_PORTRAIT_SWIG_INTERFACE_H_
