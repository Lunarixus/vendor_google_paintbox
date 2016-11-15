#ifndef GOOGLEX_GCAM_GCAM_SRC_LIB_GCAM_GCAM_H_
#define GOOGLEX_GCAM_GCAM_SRC_LIB_GCAM_GCAM_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "googlex/gcam/gcam/src/lib_gcam/debug_params.h"
#include "googlex/gcam/gcam/src/lib_gcam/gcam_callbacks.h"
#include "googlex/gcam/gcam/src/lib_gcam/init_params.h"
#include "googlex/gcam/gcam/src/lib_gcam/shot_params.h"
#include "googlex/gcam/gcam/src/lib_gcam/tuning.h"
#include "googlex/gcam/image/yuv.h"
#include "googlex/gcam/image_io/image_saver.h"
#include "googlex/gcam/image_metadata/frame_metadata.h"
#include "googlex/gcam/image_metadata/spatial_gain_map.h"
#include "googlex/gcam/image_metadata/static_metadata.h"
#include "googlex/gcam/image_raw/raw.h"

// Note: Do not #include anything else here; keep client-side
// #includes to a minimum.

// OTHER DOCUMENTATION:
//
// Most of the details you need to properly use the Gcam class are in this
// header (gcam.h) and the associated headers in the public API (referenced in
// gcam_public.h).
//
// However, there is one extra document which is required reading; be sure that
// your camera driver comply with all of the requirements in the document below,
// in order to get proper output from Gcam.
//     Guide for ISP Vendors - Gcam v1
//     https://docs.google.com/a/google.com/document/d/1Vn3R9BOf22oJZ5nVLjwTCak0Vew6KZZQjRFOG1bEscM
// TODO(geiss): Update this guide. How do these requirements map to the various
//   levels of Android Camera2 compliance?
//
// In addition, for a handy list of recommended automated compliance tests
// (to can help verify that your camera driver is capturing the right stuff,
// and setting the FrameMetadata struct properly), see this doc:
//     Gcam - Compliance Unit Tests
//     https://docs.google.com/a/google.com/document/d/1mp02rePD9tCiHwjCrMKvhFIxgPgSNcAJ3Lo1csav_QI/edit
// TODO(yuntatsai): Update this doc. How do these tests map to the CTS/ITS tests
//   already in place for Android Camera2?

namespace gcam {

class AeTraining;
class PipelineManager;
class IShot;
struct AeResults;
struct Camera;
struct ShotMemInfo;

// The main Gcam class.
// The caller should be careful to never call into the Gcam object with two
//   different threads at the same time; a mutex should be used to coordinate
//   calls between multiple threads.  Most calls are non-blocking and will
//   return quickly.
// Note that only one instance of this class should be created at a time,
//   so that future peak memory estimation is accurate.
// TODO(hasinoff): Enforce this property in Gcam::Create().
class Gcam {
 public:
  // Creates an instance of Gcam with the given parameters, supporting a list
  // of cameras with the given static metadata.
  //
  // NOTE: All API calls which require a 'camera_id' parameter refer to the
  // index within 'cameras_'. The i-th camera in the list is NOT required to
  // have a sensor ID of i, but this is often the case in practice.
  //
  // The latest tuning and noise model for each of the cameras is assumed.
  // override this behavior, UpdateCameras() may be used. (Having the ability
  // to query tuning based on old "device codes" is useful for reprocessing
  // saved bursts in their original configurations.)
  //
  // If any camera is unknown, null will be returned.
  //
  // Current devices:
  //   device           make         model            latest device code
  //   ---------------  -----------  ---------------  -------------------
  //   Google Pixel XL  "Google"     "Pixel"          "marlin"
  //   Google Pixel     "Google"     "Pixel"          "sailfish"
  //   Nexus 6P         "Huawei"     "angler"         "nexus6v2"
  //   Nexus 5X         "LGE"        "bullhead"       "nexus5v2"
  //   Nexus 6          "motorola"   "Nexus 6"        "nexus6"
  //   Nexus 5          "LGE"        "Nexus 5"        "nexus5"
  //   Glass v2         "Google"     "Glass 2"        "sand001"
  //   Glass v1         "Google"     "Glass 1"        "glass0711f"
  //   IMX214 array     "Gcam"       "Flatfish"       "array002"
  //   OV5680 array     "Gcam"       "OV5680 Array"   "array001"
  //
  // Legacy device codes:
  //   - Glass v1: "glass0711", "glass0711[b-e]"
  //   - Galaxy Nexus: "gn04d", "gn078", "gn079", "gn0711"
  //   - Before calibration: "uncalibrated"
  //
  static Gcam* Create(const InitParams& init_params,
                      const std::vector<StaticMetadata>& static_metadata_list,
                      const DebugParams* debug_params);  // Optional.

  // IMPORTANT:
  // To ensure you don't lose any images, before destructing the
  // Gcam object, block until Gcam::IsIdle() returns true.
  ~Gcam();

  // Returns whether Gcam is idle, i.e. not capturing a shot and not processing
  // any payloads in the background. (To ensure you don't lose any images, wait
  // until this is 'true' before you delete the Gcam object!)
  bool IsIdle() const;

  // Prints a brief status update.
  void PrintStatus() const;

  // Get the InitParams used to initialize Gcam.
  const InitParams& GetInitParams() const { return init_params_; }

  // (optional): Before taking a shot, the client should generally check
  //   whether the peak memory required to capture a new shot is too high.
  //
  // If peak memory is high, that's because we've buffered too many bursts for
  //   processing. Options (for the caller) for dealing with this include
  //   blocking, falling back to a non-Gcam shot, and reducing the number of
  //   frames per burst.
  //
  // Callers should either poll with PeakMemoryWithNewShotBytes() or use
  //   InitParams::memory_callback to be notified when future peak memory
  //   changes at "significant" points as bursts move through stages of
  //   processing.

  // Returns an upper bound on the future peak memory usage that Gcam would
  // reach if you were to take a single new shot -- either immediately, or at
  // any point in the future.
  //
  // Gcam's tuning is locked during normal, on-device use. If Gcam's tuning
  // isn't locked, e.g. for offline reprocessing, this memory estimate can only
  // be trusted if the next (hypothetical) shot is captured using the same
  // tuning, debug_params, etc.  If any of those are changed, then the memory
  // estimate is no longer valid.
  int64_t PeakMemoryWithNewShotBytes() const;

  // Returns the future peak memory usage for Gcam *without* any additional
  // shots, just by running all currently processing shots to completion.
  int64_t PeakMemoryBytes() const;

  //----------------------------------------------------------
  // Metering overview
  //----------------------------------------------------------
  // Gcam supports 3 different types of metering.
  //
  //   1. Metering Burst:
  //     - Involves capturing a *separate* burst on shutter press, just for
  //         metering purposes.
  //     - Metering is based on multiple frames, with different exposure levels,
  //         yielding very high-quality HDR histogram data.  (This mode is
  //         especially useful when capturing scenes for AE tagging/training
  //         purposes.)
  //     - Time-to-shot is adversely affected.
  //     - Viewfinder frames are never passed to Gcam.
  //     - On shutter button, the client should:
  //         1. Call StartShotCapture.
  //         2. Call GetMeteringBurstSpec, capture an explicit metering burst,
  //              and pass the metering frames to Gcam (via Begin/Add/
  //              EndMeteringFrame(s)).
  //         3. Capture an explicit payload burst.  (The burst spec is returned
  //              by EndMeteringFrames.)
  //         4. Pass the payload frames into Gcam, via
  //              Begin/Add/EndPayloadFrame(s).
  //     - Can be used as a fallback in case Smart Metering fails
  //         for some reason.
  //
  //   2. "Lazy" Smart Metering:
  //     - AE is computed from a single viewfinder frame -- the most recent one
  //         provided.
  //     - Much faster time-to-shot latency than non-smart metering, but still
  //         slightly slower when compared to eager smart metering.  However,
  //         relative to eager smart metering, saves power during viewfinding.
  //     - On shutter button, the client should:
  //         1. Grab (a) the latest viewfinder frame, and build (b) the
  //              AeShotParams you'd like to use for the shot.
  //         2. Call StartShotCapture.
  //              - Assign (b) to shot_params.ae.
  //              - Be sure to do this before the next step, otherwise, debug
  //                logs (AE results & timing) won't be bound to the shot.
  //         3. Call ComputeAeResults, passing in (a) and (b) from step 1.
  //         4. Call BuildPayloadBurstSpec, passing in the AeResults from step
  //              3.
  //         5. If the payload burst spec is empty (zero frames) (i.e. on
  //              failure), jump to GetMeteringBurstSpec (i.e. fall back to
  //              capturing a metering burst).
  //         6. Otherwise (on success), capture the payload burst, and jump to
  //              BeginPayloadFrames.
  //
  //   3. "Eager" Smart Metering (aka "background AE"):
  //     - AE is computed from a single viewfinder frame -- usually the most
  //         recent one provided.
  //     - Viewfinder frames must be regularly provided to Gcam (via
  //         AddViewfinderFrame), which runs AE on them in the background.
  //     - When a shot is taken, the latest AE results are grabbed and
  //         used, and the payload burst can be captured right away.
  //     - Uses more power, but has minimal time-to-shot latency.
  //     - Also provides extra data that can be useful for things like deciding
  //         whether to fire the flash or not.
  //     - On shutter button, the client should:
  //         1. Call GetLatestBackgroundAeResults and save the result.
  //         2. Call StartShotCapture.
  //              - Be sure to copy AeResults::ae_shot_params to shot_params.ae,
  //                before the call.
  //         3. Call BuildPayloadBurstSpec.
  //         4. If the payload burst spec is empty (zero frames), then jump to
  //              GetMeteringBurstSpec.
  //         5. Otherwise, capture the payload burst, and jump to
  //              BeginPayloadFrames.

  //----------------------------------------------------------
  // Optional section: Smart Metering.
  //----------------------------------------------------------

  // Smart Metering involves passing frames, during viewfinding, to Gcam.
  // When the frames passed in are raw, they are effectively
  // under-exposed in two ways:
  //     1. The red and blue channels haven't yet had WB gains applied,
  //        so we sometimes have some extra visibility into pixels that
  //        are clipped in the finished (YUV) image.
  //     2. The edges of the image haven't yet had LSC applied, so we
  //        sometimes have extra visiblity into pixels that are clipped
  //        there (in the finished image), too.
  // Then, when the user goes to take a shot, Gcam already has a lot of
  // information about the scene - enough that, sometimes, it can skip
  // the metering burst, and go straight to the payload burst.

  // AddViewfinderFrame
  // - For use with eager smart metering.  Do not call otherwise, as
  //     background AE might run on any frames passed to Gcam here.
  // - Feeds a viewfinder frame in to Gcam, then runs AE on it in the
  //     background (asynchronously).  The call is non-blocking.
  // - This function should be called continuously during viewfinding, but not
  //     every viewfinder frame needs to be passed in, e.g. this may be done at
  //     a reduced duty cycle.
  // - force_single_ae: If true, then all 3 AE modes (single, short, long) will
  //     always run (whereas, normally, short and long always run, but single
  //     only runs sometimes).  This costs slightly more CPU (on average), but
  //     will make sure that the 'single_tet' field in the returned struct is
  //     always valid.
  // - raw_id: Unique ID associated with each image. The client must ensure that
  //     memory associated remains valid until it receives a release callback
  //     for that image ID. IDs must be globally unique across all image types
  //     and be non-negative. The constant gcam::kInvalidImageId is reserved for
  //     the null image when the parameter can be invalid and will not receive a
  //     callback.
  //
  //   CLEANUP:
  //     InitParams.image_release_callback is mandatory. It will be called after
  //     gcam is done with the images, sometimes right away after copy,
  //     sometimes with a delay. The client is responsible for releasing the
  //     actual image data.
  //   ISP CONFIGURATION:
  //     Only raw viewfinder frames are supported. This means we are not
  //     affected by ISP settings like tone mapping.
  //   ROTATION:
  //     The frame should not be rotated; it should be in the original
  //     orientation, as read from the sensor.
  //   INPUT RESOLUTION:
  //     It's fine, even preferable, if the raw image is a hardware-downsampled
  //     version of the frame.  Around QVGA (320x240) is optimal for AE: high
  //     enough resolution for best-quality AE, without the expense of software
  //     downsampling.
  // Takes ownership of raw, sgm.
  // TODO(geiss, jiawen): Pass the now-required SpatialGainMap by value.
  // TODO(geiss): There's an outstanding potential bug that AdjustDigitalGain
  //   doesn't seem to be applied to frames coming in here (unlike
  //   AddMeteringFrame and AddPayloadFrame, which both call it).
  void AddViewfinderFrame(
      int camera_id,
      const bool force_single_ae,
      const FrameMetadata& metadata,
      const AeShotParams& ae_shot_params,
      int64_t raw_id,
      const RawWriteView& raw,
      const SpatialGainMap& sgm);

  // For use with eager smart metering.
  // Returns the latest results of background AE, from the viewfinder
  //   corresponding to the given sensor ID.
  // In the return struct, the 'valid' boolean will indicate if the call
  //   succeeded.
  // For this to succeed, you must be semi-regularly submitting viewfinder
  //   frames (via AddViewfinderFrame).
  // Note that these AE results are undamped.
  AeResults GetLatestBackgroundAeResults(int camera_id) const;

  // Flush the viewfinder corresponding to the given sensor ID.
  // Should only be called when using Eager Smart Metering; in this case, it
  //   will release any remaining frames (that have been submitted for
  //   background AE processing via AddViewfinderFrame) before returning.
  // Call this when the host application goes to the background, if Eager
  //   Smart Metering is in use.
  // Note that if you don't, the frames will still be released, as soon as
  //   they are background-processed; it just might happen a little more slowly.
  void FlushViewfinder(int camera_id);

  //----------------------------------------------------------
  // End section: Smart Metering.
  //----------------------------------------------------------

  //----------------------------------------------------------
  // Shot capture
  //----------------------------------------------------------

  // Returns true if one or more shots are currently in the capture phase.
  // (See also: PeakMemoryWithNewShotBytes.)
  bool IsCapturing() const;

  // Begins capture of a new shot.
  // Multiple shots can be captured at a time.
  // After this, call methods on the shot object to add frames, etc.
  // The burst ID must not have the value of kInvalidBurstId, and must be
  //   different from any other shots currently being constructed or background-
  //   processed.
  // 'postview_params' is required if any of the following conditions are met:
  //   1. InitParams.postview_callback != nullptr
  //   2. GCAM_SAVE_POSTVIEW is used
  //   3. GCAM_SAVE_MERGED is used
  // Gcam retains ownership of this object.
  // Postview images:
  //   If you would like a callback the moment a postview image is be
  //   produced, provide a valid 'postview_callback' in gcam's InitParams, then
  //   pass in a non-null PostviewParams here.  If the postview callback is
  //   valid, then PostviewParams must be non-null.
  IShot* StartShotCapture(int camera_id, int burst_id,
                          const ShotParams& shot_params,
                          // Sometimes optional:
                          const PostviewParams* postview_params,
                          // Optional:
                          const ImageSaverParams* image_saver_params);

  // IMPORTANT:
  // You must call either EndShotCapture or AbortShotCapture, exactly once, for
  // each shot successfully created via StartShotCapture.

  // Call this once your capture is complete (after Shot::EndPayloadFrames).
  // Return value:
  //   - True on success, meaning that the shot capture was successfully
  //       completed and transitioned to background processing.  You can then
  //       wait for the callback(s) (that you provided in InitParams) to be
  //       called.  InitParams.finished_callback will notify you when this shot
  //       is finished background-processing.
  //   - False on failure.  This could be because:
  //       - shot was nullptr
  //       - the shot was not being managed by Gcam
  //       - the shot had already finished capturing
  //       - EndShotCapture had already been called on the shot
  //       - AbortShotCapture had been called on the shot
  //       - the shot had a severe error during capture
  // In either case, after the call, 'shot' is invalidated, and it becomes
  //   illegal (at the public Gcam interface) to call any methods on 'shot',
  //   or to pass 'shot' to any functions.
  bool EndShotCapture(IShot* shot);

  // You must call this if you decide to abort the shot capture.
  // After this call, 'shot' is invalidated, and it becomes illegal (at the
  //   public Gcam interface) to call any methods on 'shot' or pass 'shot' to
  //   any functions.
  // Returns true on success.
  // This call (if successful) alone kills the shot; it is not necessary to call
  //   AbortShotProcessing() afterwards.
  // SEE ALSO: AbortShotProcessing.
  bool AbortShotCapture(IShot* shot);

  // Aborts background processing of the shot with the given burst_id.
  // Returns true if the abort succeeded.
  // The actual abort might or might not happen immediately (synchronously), but
  //   the return value will tell you, for certain, whether or not the shot will
  //   be aborted before any results are returned.
  //
  // If the shot hasn't finished capturing yet, that's ok; it will be
  //   auto-aborted once the capture is complete (before background processing
  //   begins).  If you want to abort the actual capture, consider calling
  //   AbortShotCapture() instead; if you do that, there is no need to call
  //   AbortShotProcessing() afterward.
  // If the capture is complete, the shot will be aborted during background
  //   processing.
  //
  // Returns false if the shot is not found.
  // Returns true if the shot is found, but was still being captured, and gets
  //   flagged to be aborted once the capture is complete.
  // Returns true if the shot is found, was done being captured, and was
  //   successfully aborted.
  // Returns false if the shot is found, but is almost finished processing,
  //   to the point gcam had already started calling result callbacks, and thus,
  //   it was too late to abort it.  In this case, the shot will be finished
  //   normally: the progress callback will reach 'kFinal', all callbacks will
  //   be called, etc.
  //
  // SEE ALSO: AbortShotCapture.
  bool AbortShotProcessing(int burst_id);

  //----------------------------------------------------------
  // Misc
  //----------------------------------------------------------

  // The methods below are for development and internal Gcam use only.
  // Normal clients probably won't have to call these.
  // TODO(hasinoff): Refactor so that these functions aren't exposed as part of
  //   the regular API.
  const Tuning& GetTuning(int camera_id) const;
  void UpdateCameras(
      const std::vector<StaticMetadata>& new_static_metadata_list,
      const std::vector<Tuning>& new_tuning_list);
  void UpdateDebugParams(const DebugParams& new_debug_params);

  // If we were to start capturing a new shot right now, this gives a
  //   conservative upper bound on the amount of memory it would use, at each
  //   stage in the pipeline.
  void GetNewShotMemEstimate(ShotMemInfo* new_shot_out) const;

 private:
  Gcam(const InitParams& init_params,
       const std::vector<StaticMetadata>& static_metadata_list,
       const std::vector<Tuning>& tuning_list,
       const DebugParams* debug_params);   // Optional.

  // Destroys and recreates the list of viewfinders using the current
  // static_metadata_list_ and tuning_list_.
  void ResetViewfinders();

  // Init-time stuff.
  // Note: Some of these are pointers mainly so that we forward-declare them,
  // hiding the details (of the classes, structs) from clients.
  // TODO(jiawen): Most of these fields should have value semantics. To hide
  // implementation details, we should use the PIMPL idiom.
  const InitParams init_params_;
  std::unique_ptr<AeTraining> ae_training_;

  // The cameras.
  std::vector<Camera> cameras_;

  // The debug parameters.
  DebugParams debug_params_;

  // Number of viewfinder frames passed in so far, using AddViewfinderFrame()
  //   *or* ComputeAeResults().
  // We count viewfinder frames over the life of the Gcam object, and do not
  //   reset this counter after each shot.
  int viewfinder_frames_received_;

  // A single object that controls all state transitions and schedules all
  // background-processing work.
  std::unique_ptr<PipelineManager> pipeline_manager_;

  friend class PayloadFrameProcessor;
  friend class Viewfinder;
};

// Return Gcam's version number, as a string taking the form
// "[major version].[google3 sync CL]", for example, "1.0.61087839".
std::string GetVersion();

}  // namespace gcam

#endif  // GOOGLEX_GCAM_GCAM_SRC_LIB_GCAM_GCAM_H_
