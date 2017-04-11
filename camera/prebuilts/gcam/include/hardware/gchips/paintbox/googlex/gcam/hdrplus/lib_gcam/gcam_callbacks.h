#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_GCAM_CALLBACKS_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_GCAM_CALLBACKS_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <cstdint>

#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/shot_log_data.h"
#include "googlex/gcam/image/pixel_format.h"
#include "googlex/gcam/image/t_image.h"

namespace gcam {

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

// Called when a burst is fully complete.
class BurstCallback {
 public:
  virtual ~BurstCallback() = default;
  virtual void Run(int burst_id, const ShotLogData& stats) const = 0;
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

// This callback notifies the client that Gcam is finished with a YuvImage*
//   or RawImage* that the client passed in to it (...a metering frame,
//   payload frame, or viewfinder frame) and is in the process of deleting it.
// If the image memory pointed to by the YuvImage* or RawImage* was not
//   created by calling the C "new" operator -- for example, if it is aliased
//   to a resource (buffer) in the graphics subsystem -- then upon receiving
//   this callback, the client should use the identifying information (burst_id,
//   frame_type, frame_index, pointer itself, or data stored in the callback
//   object) to identify the resource, and free it.
// Otherwise - if the image memory pointed to by the image was created
//   by calling the C "new" operator - then upon receiving this callback, the
//   client doesn't have to do anything.
// IMPORTANT: The client should NOT call "delete yuv" or "delete raw" in
//   this callback, as Gcam takes care of this (...that's what triggers
//   the release callback to be called, in fact).
// If you need to attach arbitrary data to the image, for use at release time,
//   you can set 'YuvImage::user_data_' or 'RawImage::user_data_' before
//   passing the frame to Gcam, and it will be propagated back to you here.
// Note: 'frame_index' indicates the zero-based index of the frame within the
//   appropriate burst (metering or payload).
// For viewfinder frames, which are part of a stream rather than a concise
//   burst, burst_id will be kInvalidBurstId, and frame_index will be set
//   to the value of 'request_.frame_number' of the 'ViewfinderFrame' object
//   that was used by the caller to pass this image in to Gcam.
class ImageReleaseCallback{
 public:
  virtual ~ImageReleaseCallback() = default;
  virtual void Run(const int64_t image_id) const = 0;
};

// Called when an image encoded in a blob of memory (DNG or JPG) is ready.
// At the end, be sure to call "delete[] data".
class EncodedBlobCallback {
 public:
  virtual ~EncodedBlobCallback() = default;
  virtual void Run(int burst_id, uint8_t* data, unsigned int bytes, int width,
                   int height) const = 0;
};

// Called at various points while processing a burst, reporting a rough
// estimate of the progress so far, 'progress', in the range [0,1].
class ProgressCallback {
 public:
  virtual ~ProgressCallback() = default;
  virtual void Run(int burst_id, float progress) const = 0;
};

// Callback to deliver an AeResults struct that was produced by Gcam in the
// background.
class BackgroundAeResultsCallback {
 public:
  virtual ~BackgroundAeResultsCallback() = default;
  virtual void Run(AeResults results) const = 0;
};

// Called when the final uncompressed image is ready.
// Only one of the two image containers (yuv_result or rgb_result)
//   will be valid, depending on the pixel_format that was requested.
// At the end, be sure to call "delete yuv_result" and "delete rgb_result".
class ImageCallback {
 public:
  virtual ~ImageCallback() = default;
  virtual void Run(int burst_id,
                   YuvImage* yuv_result,
                   InterleavedImageU8* rgb_result,
                   GcamPixelFormat pixel_format) const = 0;
};

// Postview: Settings for how Gcam should generate the postview image.
//
// The size of the returned postview image is determined by 'target_width_'
// and 'target_height_':
//   - If you specify just one (and set the other to zero): the contents (~crop
//     area) of the postview, and its aspect ratio, will match the final image.
//   - If you specify both, but the aspect ratio of the requested postview does
//     not exactly match that of the final image, the postview contents will
//     show a vertical or horizontal crop (relative to the final image).
//     This emits a warning unless 'suppress_crop_warning_' is set to true.
//
// By default, postview for HDR scenes does not have local tonemapping applied.
// To enable local tonemapping, for a higher fidelity rendition at the
// expense of speed, set the flag 'tonemap_hdr_scenes_' to true.
//
// Furthermore, by default, postview does not apply the "true long" exposure,
// even for HDR bursts that include such an exposure. To use the true long
// exposure to improve postview, by applying low-frequency color transfer, set
// the flag 'apply_true_long_' to true. This option generally slows down
// time-to-postview, since it requires waiting for the true long exposure
// (typically the last payload frame) to arrive, then doing a little more
// processing. Note that since low-frequency color transfer is part of HDR local
// tonemapping, the 'apply_true_long_' setting will only take effect if
// 'tonemap_hdr_scenes_' is also true, and if the scene warranted it.
//
// The postview is returned in a format controlled by pixel_format_.
class PostviewParams {
 public:
  PostviewParams() {}
  PostviewParams(GcamPixelFormat pixel_format, int target_width,
                 int target_height, bool suppress_crop_warning,
                 bool tonemap_hdr_scenes, bool apply_true_long)
      : pixel_format_(pixel_format),
        target_width_(target_width),
        target_height_(target_height),
        suppress_crop_warning_(suppress_crop_warning),
        tonemap_hdr_scenes_(tonemap_hdr_scenes),
        apply_true_long_(apply_true_long) {}

  GcamPixelFormat pixel_format_ = GcamPixelFormat::kUnknown;
  int target_width_ = 0;
  int target_height_ = 0;
  bool suppress_crop_warning_ = false;
  bool tonemap_hdr_scenes_ = false;  // [faster, lower quality]
  bool apply_true_long_ = false;     // [faster, worse color]
};

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_GCAM_CALLBACKS_H_
