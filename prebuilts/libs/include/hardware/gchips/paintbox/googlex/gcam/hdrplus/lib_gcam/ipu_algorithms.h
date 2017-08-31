#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_IPU_ALGORITHMS_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_IPU_ALGORITHMS_H_

#include "googlex/gcam/base/context.h"
#include "googlex/gcam/base/utils.h"
#include "googlex/gcam/image/color_matrix.h"
#include "googlex/gcam/image/t_image.h"
#include "googlex/gcam/image/yuv.h"
#include "googlex/gcam/image_metadata/bayer_pattern.h"
#include "googlex/gcam/image_metadata/static_metadata.h"
#include "googlex/gcam/image_raw/raw.h"
#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/input_frame.h"
#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/tuning.h"

namespace gcam {

// Ipu version of MeasureSharpnessRaw in
// hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_common/common.cc
std::vector<float> MeasureSharpnessRawIpu(
    const std::vector<const RawReadView*>& maps,
    const StaticMetadata& static_metadata,
    const Context& gcam_context);

// Wrapper function for MeasureSharpnessRawIpu to cluster N frames into each
// launch, where N is the maximum number of frames the IPU can compute before
// running out of LBP capacity. Only frames that are considered valid by func
// are processed.
// Note that frames is used as both an input and an output, and this
// function signature is meant to replicate the non-IPU implementation of
// the function.
void MultiLaunchMeasureSharpnessRawIpu(std::vector<InputFrameView*>* frames,
                                       const StaticMetadata& static_metadata,
                                       const Context& gcam_context);

// Ipu version of MeasureMeanSignalLevel in
// hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/tuning.cc
float MeasureMeanSignalLevelIpu(const RawReadView& map,
                                BayerPattern bayer_pattern,
                                const Context& gcam_context);

// Ipu version of RgbToPossiblyCroppedYuv in
// googlex/gcam/image/yuv_utils.cc
YuvImage RgbToPossiblyCroppedYuvIpu(const ColorMatrix& rgb2yuv,
                                    const InterleavedReadViewU8& rgb);

// Ipu version of AverageSnrFromFrame in
// hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/tuning.cc
float AverageSnrFromFrameIpu(const RawReadView& raw,
                             BayerPattern bayer_pattern,
                             float noise_model_black_level,
                             float white_level,
                             const RawNoiseModel& noise_model,
                             const Context& context);

// Ipu implementation of deinterleave for Bayer images. This is only required
// for the Ipu algorithm of align.
// No corresponding function in mainline gcam code.
// TODO(jikimjikim): Add separate use_ipu_deinterleave flag for this function.
void PrepareForAlignAndMergeIpu(
    bool use_ipu, RawBufferLayout burst_layout,
    bool do_suppression, const float black_levels[4],
    int white_level, const float awb_gains[4],
    // NOLINTNEXTLINE
    std::vector<Halide::Runtime::Buffer<const void>>& interleaved_bayers,
    std::vector<std::pair<Halide::Runtime::Buffer<uint16_t>,
        Halide::Runtime::Buffer<uint16_t>>>* deinterleaved_bayers,
    std::vector<std::pair<Halide::Runtime::Buffer<uint16_t>,
        Halide::Runtime::Buffer<uint16_t>>>* pseudo_lumas);
// Ipu implementation of image resampling.
// google3/googlex/gcam/image_proc/resample.cc
bool ResampleIpu(const InterleavedReadViewU8& src_map,
                 const Context& gcam_context,
                 float post_resample_sharpen_strength,
                 const InterleavedWriteViewU8* dst_map);

bool ResampleIpu(const YuvImage& src_map,
                 const Context& gcam_context,
                 float post_resample_sharpen_strength,
                 const YuvImage* dst_map);
}  // namespace gcam
#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_IPU_ALGORITHMS_H_
