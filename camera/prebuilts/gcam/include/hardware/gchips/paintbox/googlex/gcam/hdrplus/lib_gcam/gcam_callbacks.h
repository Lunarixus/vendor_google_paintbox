#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_GCAM_CALLBACKS_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_GCAM_CALLBACKS_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <cstdint>

#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/shot_log_data.h"
#include "googlex/gcam/image/pixel_format.h"
#include "googlex/gcam/image/t_image.h"
#include "googlex/gcam/image_metadata/exif_metadata.h"
#include "googlex/gcam/image_raw/raw.h"

namespace gcam {

class IShot;
class YuvImage;
struct AeResults;

// GCAM CALLBACK TYPES
//   To make integration with Java easier on Android by way of SWIG, Gcam's
//   callbacks are Java-style abstract classes with a Run() method (by analogy
//   with Java's Runnable interface). The client is expected to subclass these
//   classes and override the Run() method. Gcam takes these callbacks by raw
//   pointer and stores them until they are called - we do not take ownership
//   and do not delete them. In most cases, the client can simply create global
//   instances of the callback objects and pass in the address of the object.
//   If parameters need to vary at runtime, they can be added to a pool and
//   periodically garbage collected (e.g., after each shot). Java has automatic
//   garbage collection and does not have this problem (although it does need
//   to ensure that if managed code has no references to the callback object,
//   that the JNI native layer adds a global reference that is released once the
//   callback is called).

// Called after the base frame is selected.
class BaseFrameCallback {
 public:
  virtual ~BaseFrameCallback() = default;
  virtual void Run(const IShot* shot, int base_frame_index) const = 0;
};

// Called when a burst is fully complete. This callback gives the client an
// opportunity to retrieve user data before the shot object is deleted.
class BurstCallback {
 public:
  virtual ~BurstCallback() = default;
  virtual void Run(const IShot* shot, const ShotLogData& stats) const = 0;
};

// Called after various events.
class SimpleCallback {
 public:
  virtual ~SimpleCallback() = default;
  virtual void Run() const = 0;
};

// Called when future peak memory (both without and with a new shot) may have
// changed.
class MemoryStateCallback {
 public:
  virtual ~MemoryStateCallback() = default;
  virtual void Run(int64_t peak_memory_bytes,
                   int64_t peak_memory_with_new_shot_bytes) const = 0;
};

// This callback notifies the client that Gcam no longer holds a reference to
//   image with the given id. The image can now be released.
class ImageReleaseCallback{
 public:
  virtual ~ImageReleaseCallback() = default;
  virtual void Run(const int64_t image_id) const = 0;
};

// Called when an image encoded in a blob of memory (DNG or JPG) is ready.
// At the end, be sure to call "delete[] data" for DNG, or "free(data)" for JPG.
class EncodedBlobCallback {
 public:
  virtual ~EncodedBlobCallback() = default;
  virtual void Run(const IShot* shot, uint8_t* data, unsigned int bytes,
                   int width, int height) const = 0;
};

// Called at various points while processing a burst, reporting a rough
// estimate of the progress so far, 'progress', in the range [0,1].
class ProgressCallback {
 public:
  virtual ~ProgressCallback() = default;
  virtual void Run(const IShot* shot, float progress) const = 0;
};

// Callback to deliver an AeResults struct that was produced by Gcam in the
// background.
class BackgroundAeResultsCallback {
 public:
  virtual ~BackgroundAeResultsCallback() = default;
  virtual void Run(AeResults results) const = 0;
};

// Called when the merged raw image is ready.
// At the end, be sure to call "delete merged".
class RawImageCallback {
 public:
  virtual ~RawImageCallback() = default;
  virtual void Run(const IShot* shot, const ExifMetadata& metadata,
                   RawImage* merged) const = 0;
};

// Called when the final uncompressed image is ready.
// The final image is unrotated, i.e. it matches the orientation of the payload
//   images used to generate it.
// Only one of the two image containers (yuv_result or rgb_result)
//   will be valid, depending on the pixel_format that was requested.
// At the end, be sure to call "delete yuv_result" and "delete rgb_result".
class ImageCallback {
 public:
  virtual ~ImageCallback() = default;
  virtual void Run(const IShot* shot,
                   YuvImage* yuv_result,
                   InterleavedImageU8* rgb_result,
                   GcamPixelFormat pixel_format) const = 0;
};

// A collection of pointers to callback objects. All callbacks are optional
// (may be set to nullptr).
struct ShotCallbacks {
  // Invoked when the base frame has been selected. The base frame index is
  // zero-based and corresponds to the order frames were *passed to Gcam* via
  // AddPayloadFrame(), which in may be different than the order of their
  // timestamps.
  BaseFrameCallback* base_frame_callback = nullptr;

  // Invoked when Gcam generates a postview image.
  // If not nullptr, PostviewParams must also not be nullptr when calling
  // Gcam::StartShotCapture().
  ImageCallback* postview_callback = nullptr;

  // Invoked when the merged raw image is available.
  // At the moment, only RawBufferLayout::kRaw16 output is supported.
  // Guaranteed to be called before 'merged_dng_callback' below.
  RawImageCallback* merged_raw_image_callback = nullptr;

  // Invoked by the raw pipeline when a merged DNG is available.
  EncodedBlobCallback* merged_dng_callback = nullptr;

  // Invoked when the final uncompressed image is available.
  // If not nullptr, final_image_pixel_format must not be
  // GcamPixelFormat::kUnknown when calling Gcam::StartShotCapture().
  //
  // Guaranteed to be invoked before the final JPEG callback below.
  ImageCallback* final_image_callback = nullptr;

  // Invoked when the final JPEG is available.
  EncodedBlobCallback* jpeg_callback = nullptr;

  // Invoked as the pipeline makes progress.
  ProgressCallback* progress_callback = nullptr;

  // Invoked when the shot is finished. This callback will not be invoked if
  // the shot is aborted or fails during capture or background processing. After
  // this notification, the IShot will be deleted.
  BurstCallback* finished_callback = nullptr;
};

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_GCAM_CALLBACKS_H_
