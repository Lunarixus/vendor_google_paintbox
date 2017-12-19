#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_SHOT_INTERFACE_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_SHOT_INTERFACE_H_

#include <string>
#include <vector>

#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/burst_spec.h"
#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/gcam_constants.h"
#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/shot_params.h"
#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/tuning.h"
#include "googlex/gcam/image_metadata/client_exif_metadata.h"
#include "googlex/gcam/image_metadata/frame_metadata.h"
#include "googlex/gcam/image_metadata/spatial_gain_map.h"
#include "googlex/gcam/image_metadata/static_metadata.h"
#include "googlex/gcam/image_raw/raw.h"

namespace gcam {

class SaveInfo;

// TODO(geiss): For convenience, build a simpler 'SimpleShot' object, which you
// can simply pass a vector of frames, in a single call.  It will call the
// relevant methods of Shot for you, and return the resulting Shot.

// This class, Shot, contains the public interface for taking a gcam shot.
// Thread safety:
//   IShot objects have the same restriction as the Gcam object: The caller
//   should only call into an IShot object via one thread at a time.  One can
//   simultaneously call, for example, a method on the Gcam object from thread
//   1, a method on an IShot from thread 2, and a method on a different IShot
//   from thread 3; this is safe.  The IShot objects are independent from each
//   other, and are immune to concurrent changes in the Gcam object that created
//   them.
// LINT.IfChange
class IShot {
 public:
  virtual ~IShot() {}

  // Step 1: If you're using smart metering, take the latest background AE
  //   results and call BuildPayloadBurstSpec to convert it to a payload burst
  //   spec.  If a valid (non-empty) spec comes back, call ahead to
  //   BeginPayloadFrames.  If not, continue with GetMeteringBurstSpec.

  // Step 2: Ask Gcam for details on the metering burst you should capture.
  virtual BurstSpec GetMeteringBurstSpec() = 0;

  // Step 3: Start capturing the metering frames & feeding them into Gcam,
  // ideally, as the frames stream in.
  //   NON-BLOCKING:
  //     AddMeteringFrame() is non-blocking: it processes the metering
  //     frames in the background, asynchronously, on another thread.
  //     EndMeteringFrames() does block, however, until all of those
  //     frames are analyzed, and it can return BurstSpec describing
  //     the payload burst that should be captured next.
  //   DROPPED FRAMES:
  //     Gcam does not tolerate dropped *metering* frames, so if the camera
  //     driver drops a metering frame, you must call AbortShotCapture()
  //     and (if desired) auto-start a new shot.  (It does tolerate
  //     dropped payload frames; see comments above AddPayloadFrame.)
  //   RETURN VALUE:
  //     If AddMeteringFrame() returns false, or if EndMeteringFrames()
  //     returns an empty burst spec (with zero frames), then a critical error
  //     has occurred, Gcam has aborted the capture, and the client
  //     should do the same.
  //   CLEANUP:
  //     Gcam requires that non-null input image views be valid until
  //     image_release_callback (mandatory; set in InitParams) is called.
  //   METADATA:
  //     Be sure to fill out both the 'wb_capture' and 'wb_ideal'
  //     members for each frame.  'wb_capture' should tell us what
  //     color temp & gains were actually applied to the metering
  //     frames, as each incoming row was processed; and 'wb_ideal'
  //     will tell us - if available - a refined estimate of what
  //     should have been applied, once the entire frame was visible
  //     to the ISP, and could be analyzed.
  //   INPUT RESOLUTION:
  //     For optimal AE results, the incoming metering frames should be
  //     QVGA-sized (320x240).  If they are larger, they will have to
  //     be downsampled in software, which will slow processing down
  //     (increasing your time-to-shot) -- so if your ISP can give you
  //     a HW-downsampled QVGA-size version of each frame, use it.
  //     If they are smaller than QVGA, there might not be enough
  //     information, and the quality of Gcam's AE might suffer.
  //   SPATIAL GAIN MAPS:
  //     The lens shading correction (LSC) maps for the raw metering
  //       frames, corresponding to the full active area.
  //     Note that these maps are typically configured to fully correct
  //       the color shading of the lens, but to only PARTIALLY correct
  //       the vignetting of the lens.  They also might be a mixture of
  //       one or more source LSC maps for various types of canonical
  //       light sources.
  //     In general, Gcam's AE is aware of how much vignetting will be
  //       left in the shot, and exposes with this in mind.
  virtual void BeginMeteringFrames(
      const BurstSpec& metering_burst_spec) = 0;  // Required.

  // - raw_id: This is a unique ID associated with raw each image. The
  //     client must ensure that memory associated remains valid until it
  //     receives a release callback for that image ID. IDs must be globally
  //     unique across all image types and be non-negative. The constant
  //     gcam::kInvalidImageId is reserved for the null image when the
  //     parameter can be invalid and will not receive a callback.
  virtual bool AddMeteringFrame(const FrameMetadata& metadata,
                                // At least one of 'yuv' or 'raw' must be valid.
                                int64_t raw_id, const RawWriteView& raw,
                                // May be invalid if raw != nullptr.
                                const SpatialGainMap& sgm) = 0;

  virtual BurstSpec EndMeteringFrames(
      // This parameter is for internal use by Gcam.  Use the default value.
      float max_fraction_pixels_clipped = 1.0f) = 0;

  // Step 4: The client captures the requested payload burst.
  //   ISP CONFIGURATION:
  //     Same as for metering frames; see above.

  // Step 5: Feed the payload burst into Gcam as the frames stream in.
  //   NON-BLOCKING:
  //     AddPayloadFrames() is non-blocking: it processes the payload
  //     frames in the background, asynchronously, on another thread.
  //     EndPayloadFrames() does block, however, until all of those
  //     frames are taken in (although this is a very lightweight processing),
  //     and it can return a bool indicating success.
  //   DROPPED FRAMES:
  //     Gcam can tolerate dropped *payload* frames, however, on a dropped
  //     frame, you MUST still call AddPayloadFrame(), but with 'frame' set
  //     to nullptr.  (In this case, SpatialGainMap* can be nullptr, and
  //     FrameMetadata can be bogus / uninitialized.)  Gcam will emit warnings
  //     and the final quality of the shot will be reduced, but the shot will
  //     still be processed.  (We require the call, anyway, so that we can
  //     track, with certainty, *which* frames were dropped.)
  //   RETURN VALUE:
  //     If AddPayloadFrame() or EndPayloadFrames() return false, then
  //     a critical error has occurred, Gcam has aborted the capture,
  //     and the client should do the same.
  //   CLEANUP:
  //     Gcam requires that non-null input image views be valid until
  //     image_release_callback (mandatory; set in InitParams) is invoked.
  //   METADATA:
  //     Be sure to fill out both the 'wb_capture' and 'wb_ideal'
  //     members for each frame.  'wb_capture' should tell us what
  //     color temp & gains were actually applied to the payload
  //     frames, as each incoming row was processed; and 'wb_ideal'
  //     will tell us - if available - a refined estimate of what
  //     should have been applied, once the entire frame was visible
  //     to the ISP, and could be analyzed.
  //   SHARPNESS METADATA:
  //     If you already have sharpness metadata for a payload frame,
  //     store it in 'frame->meta_.sharpness', so that Gcam
  //     can skip that extra computation.  However, it is important
  //     to do this either for all frames, or for no frames (within
  //     a single payload), so that the values can be safely
  //     compared to each other (without mixing the values from the ISP's
  //     algorithm with the values from Gcam's algorithm).
  //   SPATIAL GAIN MAPS:
  //     The lens shading correction (LSC) maps for the raw payload
  //     frames, corresponding to the full active area.
  //   WARNINGS and ERRORS:
  //     Generally, if you have any warnings or errors to report for a
  //     metering or payload frame, you should add them to the warnings or
  //     errors vectors for the FrameMetadata for that frame.  However, if you
  //     have any general capture-related warnings or errors to report,
  //     that aren't tied to a specific frame, you can pass in an (optional)
  //     vector of strings, in 'general_warnings' and/or 'general_errors'.

  // For use with lazy smart metering.
  // This is a blocking call that, given a single viewfinder frame, processes it
  //   and returns the AE results.
  // In lazy smart metering, the client should hold a reference to a recent
  //   viewfinder frame, and on shutter, passes it to this function (to run AE
  //   on it) and then jumps ahead to the payload capture
  //   (BuildPayloadBurstSpec).
  // This call also updates the logging (shot_log_data_) with the new AE
  //   results, under the assumption you'll actually use these AE results to
  //   capture the shot.
  virtual AeResults ComputeAeResults(
      // The remaining parameters describe the viewfinder frame on which the
      // AE results will be based.
      const FrameMetadata& metadata,
      const RawWriteView& raw,  //  Must be valid.
      const SpatialGainMap& sgm) = 0;

  // For non-ZSL shots.
  // Constructs a spec for the payload burst, based on the given AE results and
  //   the current Gcam state (mainly the ShotParams from StartShotCapture,
  //   and the current max_payload_frames).
  // This must be called after StartShotCapture.
  // Note that the AeShotParams provided in this call (via
  //   ae_results.ae_shot_params) must exactly match those provided to
  //   StartShotCapture (via shot_params.ae).
  // This call also updates the logging (shot_log_data_) with the given AE
  //   results, under the assumption you'll actually use this BurstSpec to
  //   capture the shot.
  // Also saves debugging information for the shot, if enabled via the last call
  //   to StartShotCapture.
  virtual BurstSpec BuildPayloadBurstSpec(const AeResults& ae_results) = 0;

  // Call this before attempting to add any payload frames.
  // 'payload_burst_spec' is only required for non-ZSL shots (where some type
  //   of metering was performed, and the AeResults was passed to
  //   BuildPayloadBurstSpec to get a BurstSpec).  If the shot is ZSL, then
  //   you must pass in an empty (default) BurstSpec.
  virtual void BeginPayloadFrames(const BurstSpec& payload_burst_spec) = 0;

  // - raw_id and pd_id: A unique ID associated with each raw and PD image
  // respectively. The client must ensure that memory associated remains valid
  // until it receives a release callback for that image ID. IDs must be
  // globally unique across all image types and be non-negative. The constant
  // gcam::kInvalidImageId is reserved for invalid images, in which case the
  // client will not receive a callback.
  //
  // - pd: Raw phase detection (PD) data, with content from the left and right
  // subpixels interleaved into a single image. Optional. If provided,
  // it will be aligned and merged. If PD data is provided but the raw frame is
  // missing, the PD data will be ignored and its release callback will be
  // called immediately.
  //
  // Image release timing depends on the mode in which Gcam was initialized
  // (InitParams::payload_frame_copy_mode).
  // - PayloadFrameCopyMode::kCopyAndFreeAsap: frames are copied to internal
  //   buffers are released as soon as possible.
  // - PayloadFrameCopyMode::kDelayCopyDuringPostviewGen: frames are copied to
  //   internal buffers ASAP unless postview generation is in progress, in
  //   which case it waits until postview generation is complete.
  // - PayloadFrameCopyMode::kNeverCopy: frames are retained until the pipeline
  //   no longer references any input data.
  virtual bool AddPayloadFrame(const FrameMetadata& metadata,
                               int64_t raw_id, const RawWriteView& raw,
                               int64_t pd_id,  // Optional.
                               const InterleavedWriteViewU16& pd,  // Optional.
                               const SpatialGainMap& sgm) = 0;

  // Add metadata for an arbitrary set of frames, logged to file and MakerNote.
  // Generally these frames are not part of any burst. This extra metadata is
  // only guaranteed to be saved if this function is called before
  // EndPayloadFrames(). Saved metadata is embedded in MakerNotes EXIF tag of
  // final image. Returns true if called prior to EndPayloadFrames().
  virtual bool AddFrameMetadataForLogging(const FrameMetadata& metadata) = 0;

  // Call EndPayloadFrames once all payload frames have been submitted.
  // All parameters are optional (may be nullptr). They can be freed by the
  // client once this function returns.
  virtual bool EndPayloadFrames(
      const ClientExifMetadata* client_exif_metadata,       // Optional.
      const std::vector<std::string>* general_warnings,     // Optional.
      const std::vector<std::string>* general_errors) = 0;  // Optional.

  // Step 6:
  // Call gcam::EndShotCapture.
  // Or, if anything went wrong and the capture should be aborted, call
  //   gcam::AbortShotCapture.
  // IMPORTANT: Be sure to always call one or the other.

  // Step 7:
  // Wait for the callback(s) (that you provided in 'InitParams') to be called.
  //   InitParams.finished_callback will be the last callback, after all others
  //   are done.
  // The final image returned will be slightly center-cropped.  The current
  //   crop amount is 32 pixels on each side, plus a bit more if the incoming
  //   frames' width/height are not already multiples of 32; however, this is
  //   subject to change, so do not make assumptions about this behavior.
  // For the image results, in all versions, the caller must free the
  //   returned memory - use delete for InterleavedImageU8*, RawImage*,
  //   YuvImage*, and delete[] for a JPG or DNG encoded to memory.

  // Returns a unique id for this shot. No two IShot's will have the same id
  // over the same instantiation of the Gcam object.
  virtual int shot_id() const = 0;

  virtual SaveInfo* save() = 0;

  virtual const Tuning& tuning() const = 0;
  virtual const ShotParams& shot_params() const = 0;
  virtual const StaticMetadata& static_metadata() const = 0;

  // Advise background processing to limit CPU usage to roughly a
  //   cpu_usage_factor fraction (in [0, 1]) of peak performance
  virtual void LimitCpuUsage(float cpu_usage_factor) = 0;
};
// LINT.ThenChange(//depot/google3/java/com/google/googlex/gcam/\
//                 gcam_minimal.swig)

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_SHOT_INTERFACE_H_
