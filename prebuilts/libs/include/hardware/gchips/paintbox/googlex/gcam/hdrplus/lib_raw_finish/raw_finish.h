#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_RAW_FINISH_RAW_FINISH_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_RAW_FINISH_RAW_FINISH_H_

#include <memory>
#include <functional>

#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/gcam_types.h"
#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/tuning.h"
#include "googlex/gcam/image/t_image.h"
#include "googlex/gcam/image_metadata/awb_info.h"
#include "googlex/gcam/image_metadata/spatial_gain_map.h"
#include "googlex/gcam/image_metadata/static_metadata.h"
#include "googlex/gcam/tonemap/tonemap_yuv.h"

namespace gcam {

class SaveInfo;
struct Context;

using FinishProgressCallback = std::function<bool(Stage, float)>;

// Implements the finishing stages of the image pipeline:
// - Black level subtraction/normalization.
// - Transform Bayer pattern to RGGB.
// - Apply gains to the images (digital gain, white balance gains, lens shading
//   correction map).
// - Demosaic.
// - Apply color correction matrices.
// - Apply tone mapping curve 'output_tonemap'.
// - Boost saturation.
//
// This function expects that 'frame' contains linear data, where black is the
// per-channel value in 'black_level_offsets' and white is 'white_level'.
//
// 'white_level' must be in the range [kRawFinishMinInputWhiteLevel ..
// kRawFinishMaxInputWhiteLevel].
//
// The result is a tonemapped 8 bit image.
//
// If report_progress_func returns false, processing will be aborted and a null
// image returned.
InterleavedImageU8 FinishRaw(
    const RawFinishParams& params,
    const StaticMetadata& merged_static_metadata,
    const FrameMetadata& merged_metadata,
    float merged_avg_snr,
    // The spatial gain map to apply to the image. This must be cropped to cover
    // the output image, i.e. the corners of this spatial gain map map to the
    // corners of the output.
    const SpatialGainMap& spatial_gain_map,
    const AwbInfo& wb_ideal,
    // Gain to apply to the image during this processing.
    float gain,
    // The maximum # of synthetic exposures to use, or <= 0 if no limit.
    int max_synthetic_exposures,
    // The TET of the input frame.  Note that the input doesn't yet have the
    // gain map applied, so more digital gain (which may be baked into the gain
    // map) could still be to come.
    float input_image_tet,
    // For HDR shots, this is the desired TET of the short exposure; for non-HDR
    // shots, it is the desired TET of the final shot.
    float short_desired_tet,
    // For HDR shots, this is the desired TET of the long exposure; for non-HDR
    // shots, it is unused (set to 0).
    float long_desired_tet,
    // From AeResults.  The fraction of pixels that are likely to come from the
    // long synthetic exposure, during exposure fusion.
    float pure_fraction_of_pixels_from_long_exposure,
    // Analog and digital gains that have already been applied to the input
    // image.
    float base_frame_analog_gain,
    float base_frame_digital_gain,
    float sensitivity,
    // Crop to apply to the output image, or empty if no crop is desired.
    const PixelRect& crop,
    const Context& gcam_context,
    FinishProgressCallback report_progress_func,
    std::unique_ptr<PlanarImageU16> frame,
    // The relative scaling of the image being processed. This adjusts tuning
    // of processing that depends on absolute spatial frequencies. For example,
    // to make the processing for FinishRaw of an image A, and image A
    // downsampled by 2x, set scale_factor to 0.5 for the downsampled image.
    // TODO(dsharlet): This could be used to adjust the tuning of sharpening and
    // other processing, it is currently only used by HDR.
    float scale_factor,
    bool use_bgu,
    // Determine architecture (CPU/HVX/IPU) and other conditions to run with
    ExecuteOn *execute_on,
    // Optional.
    SaveInfo* save,
    // Optional output.
    int* synthetic_exposure_count = nullptr);

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_RAW_FINISH_RAW_FINISH_H_
