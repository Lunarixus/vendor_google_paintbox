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
// `id` is a generic unique identifier and intentionally a signed int for Java
// compatibility.
//
// `progress` is in the range [0, 1] and is expected to increase monotonically
// at each invocation. `progress` will be reported at 1.0f upon completion.
class GoudaProgressCallback {
 public:
  virtual ~GoudaProgressCallback() = default;
  virtual void Run(int64_t id, float progress) = 0;
};

// GoudaImageCallback's member functions are invoked when the final uncompressed
// image is ready. Depending on the pixel formats requested, more than one
// member function may be invoked.
//
// For both callbacks, it is the client's responsibility to call delete `image`.
// (We would prefer to use std::unique_ptr. However, our client is a Java
// Android app and SWIG does not yet support std::unique_ptr across languages).
//
// `id` is a generic unique identifier and intentionally a signed int for Java
// compatibility.
//
// `pixel_format` determines the precise pixel format and byte ordering in
// `image`.
//
// `description` is a concise, potentially human-facing description of the image
// being delivered--e.g., "input_image", "raw_pd", "disparity",
// "face_detections", etc.  Description strings should only contain characters
// suitable for use in filenames, i.e., a-zA-Z0-9_.
class GoudaImageCallback {
 public:
  virtual ~GoudaImageCallback() = default;

  virtual void RgbReady(int64_t id, gcam::InterleavedImageU8* image,
                        gcam::GcamPixelFormat pixel_format,
                        const std::string& description) = 0;

  virtual void YuvReady(int64_t id, gcam::YuvImage* image,
                        gcam::GcamPixelFormat pixel_format,
                        const std::string& description) = 0;
};

// GoudaOutputFeaturesCallback's member functions are invoked just prior to the
// PortraitProcessor exiting.
//
// `id` is a generic unique identifier and intentionally a signed int for Java
// compatibility.
//
// `description` is a human-readable string describing a feature produced by
// the portrait processor. It is logged for debugging purposes. Possible
// features are the face size fraction, focus distance or statistics of the
// disparity histogram.
//
// `value` is the value of the feature described by `description`.
class GoudaOutputFeaturesCallback {
 public:
  virtual ~GoudaOutputFeaturesCallback() = default;
  virtual void AddFeature(
      int64_t id, const std::string& description, double value) = 0;
};

// GoudaCompleteCallback::Run is invoked after all other callbacks for the
// object with unique identifier `id` have been invoked.
//
// `id` is a generic unique identifier and intentionally a signed int for Java
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

  // Invoked when the upsampled input image is available. This is useful for
  // accelerating the portrait processing pipeline--normally HDR+ upsamples
  // digital zoomed shots up to the full sensor resolution, but the extra pixels
  // just slow down portrait processing. Instead, HDR+ can produce a native
  // resolution cropped image and defer upsampling to the portrait processor,
  // which will return an upsampled HDR+ image via this callback. This will be
  // invoked only once very early in the processing pipeline to mitigate the
  // risk of losing images due to something killing the post-processing task.
  GoudaImageCallback* upsampled_input_image_callback = nullptr;

  // Invoked when the primary output image is available. This will be invoked
  // only once.
  GoudaImageCallback* image_callback = nullptr;

  // Invoked once for each secondary output image as it becomes available.
  // Secondary outputs are images that could conceivably be surfaced to users as
  // results, e.g., different blur amounts, cropped subimages, etc. This may be
  // invoked any number of times (including zero) before the complete_callback
  // is invoked.
  GoudaImageCallback* secondary_image_callback = nullptr;

  // Invoked once for each debug image as it becomes available. This may be
  // invoked any number of times (including zero) before the complete_callback
  // is invoked.
  GoudaImageCallback* debug_image_callback = nullptr;

  // Invoked once when all background processing is complete. Contains features
  // and metadata created during processing that may be useful for debugging.
  GoudaOutputFeaturesCallback* features_callback = nullptr;

  // Invoked when background processing is complete and no more callbacks will
  // be invoked.
  GoudaCompleteCallback* complete_callback = nullptr;
};

}  // namespace creative_camera

#endif  // GOOGLEX_GCAM_CREATIVE_CAMERA_PORTRAIT_MODE_PORTRAIT_CALLBACKS_H_
