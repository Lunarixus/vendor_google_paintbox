#ifndef GOOGLEX_GCAM_CREATIVE_CAMERA_PORTRAIT_MODE_PORTRAIT_CALLBACKS_H_
#define GOOGLEX_GCAM_CREATIVE_CAMERA_PORTRAIT_MODE_PORTRAIT_CALLBACKS_H_

#include <cstdint>

#include "googlex/gcam/image/pixel_format.h"
#include "googlex/gcam/image/t_image.h"
#include "googlex/gcam/image/yuv.h"

namespace creative_camera {

// GoudaProgressCallback::Run is invoked at various points during procressing to
// report a rough estimate of the progress so far.
//
// 'id' is a generic unique identifier and intentionally a signed int for Java
// compatibility.
//
// 'progress' is in the range [0, 1] and is expected to increase monotonically
// at each invocation. 'progress' will be reported at 1.0f upon completion.
class GoudaProgressCallback {
 public:
  virtual ~GoudaProgressCallback() = default;
  virtual void Run(int64_t id, float progress) = 0;
};

// GoudaImageCallback's member functions are invoked when the final uncompressed
// image is ready. Depending on the pixel formats requested, more than one
// member function may be invoked.
//
// For both callbacks, it is the client's responsibility to call 'delete image'.
// (We would prefer to use std::unique_ptr. However, our client is a Java
// Android app and SWIG does not yet support std::unique_ptr across languages).
//
// 'id' is a generic unique identifier and intentionally a signed int for Java
// compatibility.
//
// 'pixel_format' determines the precise pixel format and byte ordering in
// 'image'.
class GoudaImageCallback {
 public:
  virtual ~GoudaImageCallback() = default;

  virtual void RgbReady(int64_t id, gcam::InterleavedImageU8* image,
                        gcam::GcamPixelFormat pixel_format) = 0;

  virtual void YuvReady(int64_t id, gcam::YuvImage* image,
                        gcam::GcamPixelFormat pixel_format) = 0;
};

// GoudaCompleteCallback::Run is invoked after all other callbacks for the
// object with unique identifier 'id' have been invoked.
//
// 'id' is a generic unique identifier and intentionally a signed int for Java
// compatibility.
//
// Run() will only ever be invoked at most once (upon completion).
class GoudaCompleteCallback {
 public:
  virtual ~GoudaCompleteCallback() = default;
  virtual void Run(int64_t id) = 0;
};

// A collection of pointers to callback objects. All callbacks are optional
// (may be set to nullptr).
struct GoudaCallbacks {
  // Invoked as background processing makes progress.
  GoudaProgressCallback* progress_callback = nullptr;

  // Invoked when an output image is available.
  GoudaImageCallback* image_callback = nullptr;

  // Invoked when background processing is complete and no more callbacks will
  // be invoked.
  GoudaCompleteCallback* complete_callback = nullptr;
};

}  // namespace creative_camera

#endif  // GOOGLEX_GCAM_CREATIVE_CAMERA_PORTRAIT_MODE_PORTRAIT_CALLBACKS_H_
