#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_INIT_PARAMS_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_INIT_PARAMS_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <string>

#include "googlex/gcam/base/log_level.h"
#include "googlex/gcam/base/file_saver.h"
#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/gcam_callbacks.h"
#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/gcam_types.h"
#include "googlex/gcam/image/allocator.h"
#include "googlex/gcam/image/pixel_format.h"

namespace gcam {

extern const char* const kInitParamsFilename;

enum PayloadFrameCopyMode {
  // Use this mode if your ISP has a small circular buffer -- one that is not
  // sufficient to hold the payload frames for the longest burst that might
  // be captured.
  //
  // In this mode, Gcam will copy the payload frames (and free the originals)
  // as quickly as it can -- even during the generation of the postview (which
  // can slow down postview generation).  (The copying is asynchronous - so
  // that EndPayloadFrames can be a non-blocking call - and happens in a
  // background Gcam thread.)
  //
  // IMPORTANT: There is no guarantee that Gcam's asynchronous copying will be
  // able to keep up, and if/when it can't, if the client's buffers are all
  // full, the client will have to deal with this (presumably by delaying
  // further capture until Gcam releases another frame).
  //
  // (+) Allows for a smaller circular frame buffer in ISP.
  // (-) Could delay delivery of postview image!
  // (-) Copies each payload frame once (uses more power AND memory).
  // (+) Rapid shots: User can capture another shot quickly (without waiting
  //       for align & merge to complete).
  kCopyAndFreeAsap = 0,

  // Use this mode if your ISP has enough memory carved out to hold all of the
  // payload frames, but your device isn't super-fast (i.e. if align + merge,
  // together, take over 500 ms).
  //
  // This mode will delay frame-copying when the postview is either about to
  // be started, or is currently being produced, to ensure that when the CPU
  // is busy trying to render a postview image, it's not slowed down by any
  // background frame-copying.  Gcam will still copy and release each frame,
  // eventually, before the align & merge step, so that if the user wants to
  // take another shot (during align & merge), the ISP's buffers are all free
  // and ready.
  //
  // (-) Requires more of the ISP's memory.
  // (+) No delay to delivery of postview image.
  // (-) Copies each payload frame once (uses more power AND memory).
  // (+) Rapid shots: User can capture another shot quickly (without waiting
  //       for align & merge to complete).
  kDelayCopyDuringPostviewGen = 1,

  // Use this mode if your ISP has enough memory carved out to hold all of the
  // payload frames, and if your device is super-fast (i.e. if align + merge,
  // together, take under 500 ms); *** or if your payload frames are allocated
  // from general system memory, rather than precious/limited ISP memory. ***
  //
  // You might also want to use this mode if you want to reduce peak memory
  // usage (~the payload frame copies), and are willing to block the user
  // from taking another shot until align & merge completes.
  //
  // This mode skips the frame-copying entirely, saving power.  But, if your
  // ISP has limited memory in which to store payload frames, the user might
  // not be able to take a second shot until the first one is fully aligned
  // and merged, and the payload frames are all released.
  //
  // NOTE: In this mode, it is the client's responsibility to block the user
  // from taking a new shot, until the prior shot finishes the align & merge
  // stage (...unless you have infinite ISP memory).  To get this notification,
  // set 'merge_queue_empty_callback' in InitParams.
  //
  // (-) Requires a larger circular frame buffer in ISP.
  // (+) No delay to delivery of postview image.
  // (+) Never copies the payload frames (less power AND memory).
  // (-) Rapid shots: Caller will have to block user from taking next shot
  //       until all payload frame release callbacks (from prior shot) have
  //       been called.
  kNeverCopy = 2,

  // Invalid setting; do not use.
  kInvalidPayloadFrameCopyMode = 3
};

// Parameters for Gcam initialization time.
// On construction, contains a good set of defaults you can use.
// *Important*: although we do save these parameters when saving all burst
// inputs, we don't re-load them when re-processing bursts.  As such, none
// of these settings should affect the final output of Gcam.
//
// See also:
//   InitParams GetRandomInitParams()
//
struct InitParams {
  InitParams() { Clear(); }
  void Clear();  // Applies default settings.
  bool Check() const;
  void Print(LogLevel log_level) const;
  void SerializeToString(std::string* str) const;
  // Initialize from the string (which is presumed to come from a previous
  // call to SerializeToString).  On failure, returns false and leaves the
  // InitParams in a partially-initialized state.
  bool DeserializeFromString(const std::string& str);
  bool Equals(const InitParams& other) const;

  int thread_count;  // Recommended: Max # of CPU cores available.
  bool verbose;

  // Whether to force-disable smart metering. When smart metering is disabled,
  // related API calls will fail and no Viewfinder object will be created.
  // Default: false.
  bool disable_smart_metering;

  // The minimum number of metering frames, if we capture a full metering sweep.
  // Must be >= kMinFullMeteringSweepFrames.
  int min_full_metering_sweep_frames;
  // The maximum number of metering frames, if we capture a full metering sweep.
  // Must be <= kMaxFullMeteringSweepFrames.
  int max_full_metering_sweep_frames;

  // The minimum number of frames in a payload burst.
  // Must be >= kMinPayloadFrames.
  // This parameter mainly determines the minimum number of frames that will
  //   be requested for a non-ZSL shot. For ZSL shots, the client may provide
  //   fewer frames.
  // If either a ZSL or non-ZSL payload burst are provided to gcam with fewer
  //   frames, gcam will issue a warning (in both cases), but still process the
  //   shot to the best of its ability.
  int min_payload_frames;

  // The maximum number of frames in a payload burst.
  // Must be >= min_payload_frames.
  // For ZSL shots we may capture more frames than this, but a corresponding
  //   number of extra frames will be discarded. The number of payload frames
  //   processed still obeys this limit.
  int max_payload_frames;

  // The maximum number of frames for a ZSL shot.
  // Must be >= kMinPayloadFrames.
  // This limit applies in a "soft" way. Any ZSL frames beyond this limit
  //   immediately get discarded with a warning.
  // This limit is allowed to exceed 'max_payload_frames', in which case the
  //   difference (max_zsl_frames - max_payload_frames) determines how many
  //   *additional* blurry frames to discard.
  int max_zsl_frames;

  // This allows you to specify when (and if) Gcam copies and releases the
  // payload frames passed into it.
  PayloadFrameCopyMode payload_frame_copy_mode;

  // Whether to allow processing of images from unknown devices. This flag
  // should be false in production. If the flag is true and we encounter an
  // unknown device, we log a warning and fall back to "uncalibrated" tuning.
  // This behavior may be useful for reprocessing bursts from more exotic
  // sources (e.g. dSLR's or simulated data).
  // Default: false.
  bool allow_unknown_devices;

  // Whether to lock device tuning to the cameras described at initialization.
  // This flag should be true in production. Setting this flag to false may be
  // useful for offline reprocessing, to handle a variety of device types using
  // the same Gcam object,
  // Default: true.
  bool tuning_locked;

  // Allow Gcam to use the Hexagon DSP to accelerate processing, when available.
  // This is enabled by default when Hexagon is available.
  bool use_hexagon;

  // Allow Gcam to use the IPU to accelerate processing.
  bool use_ipu;

  // Whether to allow two different shots to simultaneously *execute* in the
  //   merge and finish stages.
  // This is mainly for devices where you plan to use (for example) HVX for
  //   finish and the CPU for merge, so you can get roughly double throughput.
  // Note that even when this is false, Gcam can* have one shot in each of the
  //   Merge and Finish stages at the same time, albeit with one paused.  (*If
  //   shot stage priorities are set such that merge is prioritized over finish,
  //   in memory_estimation.h, then finish can be paused while a merge
  //   executes.)
  bool simultaneous_merge_and_finish;

  // Thread priority levels for various Gcam operations.
  //
  // By default, capture inherits the priority of the parent thread, and merge
  // and finish are explicitly set to different background priority levels (with
  // merge slightly higher priority than finish, to reduce the memory footprint
  // more quickly). The priorities of merge, and finish *must* be set
  // explicitly.
  //
  ThreadPriority capture_priority;
  ThreadPriority merge_priority;
  ThreadPriority finish_priority;

  // This allows you to pass in custom versions of malloc and free,
  // to be used for the allocation of image data.
  MallocFunc custom_malloc;
  FreeFunc   custom_free;

  // This allows you to pass in a custom file saving object, to be used for
  // saving image data and sidecar metadata for debugging (if the various
  // GCAM_DEBUG_* flags are active).  Gcam does not take ownership of this
  // object and expects it to persist until after the Gcam object is destroyed.
  FileSaver* custom_file_saver;

  // Callback ownership:
  // Gcam does not take ownership of callback objects passed in and expects them
  // to persist until after the Gcam object is destroyed.

  // memory_callback: Called when Gcam future peak memory usage changes at
  // "significant" points as bursts move through stages of processing. May also
  // be called when peak memory has not changed.
  //
  // The peak memory values reported by Gcam are conservative, and are
  // guaranteed to go down monotonically until a new shot is captured. The
  // memory callback will be triggered after capturing a new shot, which
  // increases future peak memory. It will also be triggered as that shot or
  // previous shots work through stages of the processing pipeline; each such
  // transition potentially reduces future peak memory.
  //
  // This callback may be called repeatedly due to events during background
  // processing. It is possible that this callback will be called concurrently
  // by multiple threads.
  MemoryStateCallback* memory_callback;

  // merge_queue_empty_callback:
  //   Called when all queued captures are merged.  Note that one or more
  //   merged image(s) likely still need to be finished, which Gcam will
  //   continue to do, in the background.  However, if you clocked the
  //   CPU up at StartShotCapture, then this is a good time to clock it
  //   back down.)
  SimpleCallback* merge_queue_empty_callback;

  // finish_queue_empty_callback:
  //   Called when all merged images are finished with post-processing,
  //   causing Gcam to go idle.
  SimpleCallback* finish_queue_empty_callback;

  // This will be called when using Smart Metering with background AE enabled,
  //   whenever new background AE results are available -- or when too much time
  //   has elapsed (since a new viewfinder frame was passed in) and they go
  //   stale.
  // NOTE: Be sure to check the 'valid' bool in the returned AeResults struct,
  //   as it will tell you if these are new valid results, or if this is an
  //   expiration of stale results.
  // You can use the returned AeResults struct to:
  //   - Help determine if Gcam should be used for the shot.  (If memory
  //       permits, and if run_hdr is true or log_scene_brightness is low.)
  //   - Determine if flash should be used or not, for Gcam shots.
  BackgroundAeResultsCallback* background_ae_results_callback;

  // Required: called whenever Gcam is finished with an input image.
  // This comprises viewfinder, metering, and payload frames.
  ImageReleaseCallback* image_release_callback;
};

InitParams GetRandomInitParams();

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_INIT_PARAMS_H_
