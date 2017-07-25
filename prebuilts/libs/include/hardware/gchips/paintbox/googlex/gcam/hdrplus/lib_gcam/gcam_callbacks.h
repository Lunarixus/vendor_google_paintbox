#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_GCAM_CALLBACKS_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_GCAM_CALLBACKS_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <cstdint>

#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/shot_log_data.h"
#include "googlex/gcam/image/pixel_format.h"
#include "googlex/gcam/image/t_image.h"
#include "googlex/gcam/image_metadata/exif_metadata.h"
#include "googlex/gcam/image/yuv.h"
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
  virtual void Run(const IShot* shot, int base_frame_index) = 0;
};

// Called when a burst is fully complete. This callback gives the client an
// opportunity to retrieve user data before the shot object is deleted.
class BurstCallback {
 public:
  virtual ~BurstCallback() = default;
  virtual void Run(const IShot* shot, const ShotLogData& stats) = 0;
};

// Called after various events.
class SimpleCallback {
 public:
  virtual ~SimpleCallback() = default;
  virtual void Run() = 0;
};

// Called when the pipeline needs to report an error.
// TODO(jiawen): Consider switching all other callbacks to ids. In practice, we
// never used the IShot object anyway besides retrieving its id, the id was only
// used to look up Java-side in-flight shot params.
class ShotErrorCallback {
 public:
  virtual ~ShotErrorCallback() = default;
  virtual void Run(int shot_id, const std::string& message) = 0;
};

// Called when future peak memory (both without and with a new shot) may have
// changed.
class MemoryStateCallback {
 public:
  virtual ~MemoryStateCallback() = default;
  virtual void Run(int64_t peak_memory_bytes,
                   int64_t peak_memory_with_new_shot_bytes) = 0;
};

// This callback notifies the client that Gcam no longer holds a reference to
//   image with the given id. The image can now be released.
class ImageReleaseCallback{
 public:
  virtual ~ImageReleaseCallback() = default;
  virtual void Run(const int64_t image_id) = 0;
};

// Called when an image encoded in a blob of memory (DNG or JPG) is ready.
// At the end, be sure to call "delete[] data" for DNG, or "free(data)" for JPG.
class EncodedBlobCallback {
 public:
  virtual ~EncodedBlobCallback() = default;
  virtual void Run(const IShot* shot, uint8_t* data, unsigned int bytes,
                   int width, int height) = 0;
};

// Called at various points while processing a burst, reporting a rough
// estimate of the progress so far, 'progress', in the range [0,1].
class ProgressCallback {
 public:
  virtual ~ProgressCallback() = default;
  virtual void Run(const IShot* shot, float progress) = 0;
};

// Callback to deliver an AeResults struct that was produced by Gcam in the
// background.
class BackgroundAeResultsCallback {
 public:
  virtual ~BackgroundAeResultsCallback() = default;
  virtual void Run(const AeResults& results) = 0;
};

// Callback to deliver PD updates.
class PdImageCallback {
 public:
  virtual ~PdImageCallback() = default;

  // Invoked when the merged PD data is available. 'merged_pd' is guaranteed to
  // be not null. The client takes ownership of 'merged_pd' and must call
  // "delete merged_pd" when it is finished with the image.
  //
  // PD data is vertically upsampled by a factor of two to Bayer plane
  // resolution. If a crop is requested by the client, the merged PD will only
  // contain data within that crop.
  virtual void ImageReady(const IShot* shot,
                          gcam::InterleavedImageU16* merged_pd) = 0;

  // Invoked when the attempt to merge PD data fails.
  virtual void MergePdFailed(const IShot* shot) = 0;
};

// Callback to deliver merged RAW images.
class RawImageCallback {
 public:
  virtual ~RawImageCallback() = default;

  // Invoked when the merged RAW image is ready. 'merged_raw' is guaranteed to
  // be not null. The client takes ownership of 'merged_raw' and must call
  // "delete merged_raw" when it is finished with the image.
  virtual void ImageReady(const IShot* shot, const ExifMetadata& metadata,
                          RawImage* merged_raw) = 0;

  // Invoked when the merged RAW image is ready and the client passed in a
  // preallocated buffer. The release callback for the preallocated buffer will
  // be invoked once this function returns.
  virtual void PreallocatedReady(const IShot* shot,
                                 const ExifMetadata& metadata,
                                 const RawReadView& merged_raw) = 0;
};

// When the final uncompressed image is ready, exactly one of the member
// functions will be invoked, depending on the format and buffer allocation
// selected when the 'shot' was started. When invoked, we guarantee that the
// function arguments will not be nullptr.
//
// The final image will be in the same orientation as the payload image(s) used
// to generate it.
class FinalImageCallback {
 public:
  virtual ~FinalImageCallback() = default;

  // After this function returns, the preallocated buffer corresponding to
  // 'image_view' will be released (i.e., ImageReleaseCallback::Run() will be
  // invoked).
  virtual void PreallocatedRgbReady(const IShot* shot,
                                    const InterleavedReadViewU8& image_view,
                                    const ExifMetadata& metadata,
                                    GcamPixelFormat pixel_format) = 0;

  // After this function returns, the preallocated buffer corresponding to
  // 'image_view' will be released (i.e., ImageReleaseCallback::Run() will be
  // invoked).
  virtual void PreallocatedYuvReady(const IShot* shot,
                                    const YuvReadView& image_view,
                                    const ExifMetadata& metadata,
                                    GcamPixelFormat pixel_format) = 0;

  // The client is responsible for deleting 'image' with "delete image".
  virtual void RgbReady(const IShot* shot, InterleavedImageU8* image,
                        const ExifMetadata& metadata,
                        GcamPixelFormat pixel_format) = 0;

  // The client is responsible for deleting 'image' with "delete image".
  virtual void YuvReady(const IShot* shot, YuvImage* image,
                        const ExifMetadata& metadata,
                        GcamPixelFormat pixel_format) = 0;
};

// TODO(ruiduoy): Get the client pass in a write view for the postview too.
//   This will let us unify {Postview,Final}Callback
// Called when the postview image is ready.
// The postview image is unrotated, i.e. it matches the orientation of the
//   payload image used to generate it.
// Only one of the two image containers (yuv_result or rgb_result)
//   will be valid, depending on the pixel_format that was requested.
// At the end, be sure to call "delete yuv_result" and "delete rgb_result".
class PostviewCallback {
 public:
  virtual ~PostviewCallback() = default;
  virtual void Run(const IShot* shot,
                   YuvImage* yuv_result,
                   InterleavedImageU8* rgb_result,
                   GcamPixelFormat pixel_format) = 0;
};

// A collection of pointers to callback objects. All callbacks are optional
// (may be set to nullptr).
struct ShotCallbacks {
  // Invoked when the pipeline reports an error.
  ShotErrorCallback* error_callback = nullptr;

  // Invoked when the base frame has been selected. The base frame index is
  // zero-based and corresponds to the order frames were *passed to Gcam* via
  // AddPayloadFrame(), which in may be different than the order of their
  // timestamps.
  BaseFrameCallback* base_frame_callback = nullptr;

  // Invoked when Gcam generates a postview image.
  // If not nullptr, PostviewParams must also not be nullptr when calling
  // Gcam::StartShotCapture().
  PostviewCallback* postview_callback = nullptr;

  // Invoked when the merged raw image is available.
  // At the moment, only RawBufferLayout::kRaw16 output is supported.
  // Guaranteed to be called before 'merged_dng_callback' below.
  RawImageCallback* merged_raw_image_callback = nullptr;

  // Invoked when the merged PD data is available.
  PdImageCallback* merged_pd_callback = nullptr;

  // Invoked by the raw pipeline when a merged DNG is available.
  EncodedBlobCallback* merged_dng_callback = nullptr;

  // Invoked when the final uncompressed image is available.
  // If not nullptr, final_image_pixel_format must not be
  // GcamPixelFormat::kUnknown when calling Gcam::StartShotCapture().
  //
  // Guaranteed to be invoked before the final JPEG callback below.
  FinalImageCallback* final_image_callback = nullptr;

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
