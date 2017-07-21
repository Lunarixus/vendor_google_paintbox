#ifndef GOOGLEX_GCAM_IMAGE_PROC_RESAMPLE_H_
#define GOOGLEX_GCAM_IMAGE_PROC_RESAMPLE_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include "googlex/gcam/image/t_image.h"
#include "googlex/gcam/image/yuv.h"
#include "googlex/gcam/image_raw/raw.h"

namespace gcam {

struct Context;

// Resample an image using Lanczos interpolation.  Optionally sharpen the
// image after resampling (post_resample_sharpen_strength == 0 means no
// sharpening).
bool ResampleLanczos(const InterleavedReadViewU8& src_map,
                     const Context& gcam_context,
                     float post_resample_sharpen_strength,
                     const InterleavedWriteViewU8* dst_map);
inline bool ResampleLanczos(const InterleavedReadViewU8& src_map,
                            const Context& gcam_context,
                            const InterleavedWriteViewU8* dst_map) {
  return ResampleLanczos(src_map, gcam_context, 0.0f, dst_map);
}

// The following variants use the default Halide threadpool, rather than the one
// specified by a gcam::Context.
bool ResampleLanczos(const InterleavedReadViewU8& src_map,
                     float post_resample_sharpen_strength,
                     const InterleavedWriteViewU8* dst_map);
inline bool ResampleLanczos(const InterleavedReadViewU8& src_map,
                            const InterleavedWriteViewU8* dst_map) {
  return ResampleLanczos(src_map, 0.0f, dst_map);
}

// Low quality nearest-neighbor resampling.
bool ResampleNearest(const InterleavedReadViewU8& src_map,
                     const InterleavedWriteViewU8* dst_map);
bool ResampleNearest(const YuvReadView& src_map, const YuvWriteView* dst_map);

// Compute the minimum number of times to cut an image in half (on both axes)
// until the number of pixels is reduced to some maximum pixel count or lower.
// If the size cannot be reduced sufficiently with an exact (2^N x 2^N):1
// downsample, returns -1 and prints an error. Otherwise, returns a value >= 0.
int GetDownsampleIterations(const YuvReadView& yuv,
                            int desired_max_pixel_count);
int GetDownsampleIterations(const RawReadView& raw,
                            int desired_max_pixel_count);

// Successively downsamples by a factor of two, downsample_iterations times,
// i.e. the resulting image will have a resolution that is reduced by a factor
// of (1 << downsample_iterations) in both dimensions. Results are rounded to
// nearest.
//
// We currently only support downsampling up to 16x16:1, i.e. downsample_bits
// <= 4. If we try to integrate over larger areas, we would overflow our 16-bit
// accumulators.
bool Downsample(const InterleavedReadViewU8& src_image,
                int downsample_iterations, const Context& gcam_context,
                const InterleavedWriteViewU8* dst_image);
InterleavedImageU8 Downsample(const InterleavedReadViewU8& src_image,
                              int downsample_iterations,
                              const Context& gcam_context);
bool Downsample(const YuvReadView& source_yuv, int downsample_bits,
                const Context& gcam_context, YuvWriteView* dest_yuv);
YuvImage Downsample(const YuvReadView& source_yuv, int downsample_bits,
                    const Context& gcam_context);

// Specialized function to downsample an RGB image 8:1.
// Out of every 8x8 pixels, it only adds up the even rows (0,2,4,6) in order to
//   cut the memory bandwidth in half.
InterleavedImageU8 DownsampleRgb8to1(const InterleavedReadViewU8& rgb_in,
                                     const Context& gcam_context);

// Downsamples using a box filter, where the "box" is a rectangular
// integration over the source pixels, and has sub-pixel placement
// precision.
bool SmoothDownsample(const InterleavedReadViewU8& src,
                      const InterleavedWriteViewU8& dst);

// Bilinearly downsamples a Bayer raw image to a lower-resolution Bayer raw
//   image.
// ** Primarily used for raw postview generation; behavior and performance are
//   designed to favor that use case. **
// The function is designed mainly for downsampling, where it uses a box filter.
//   It will also *work* for upsampling, although it does not use bilinear
//   interpolation in that case; it just takes nearest-neighbor.
// The input image is a full-sized raw image, which may be packed.
// The output image is an unpacked, generally lower-resolution, 4-channel planar
//   image consisting of the Bayer color planes.
// The input image is densely sampled, to make sure that as little noise as
//   possible is left in the image.  This is because the postview image will
//   not go through any further denoising, and if any noise is left in it, it
//   create a color halo (adding a color tint where the SGM is strongest) due to
//   the noise remaining after BLS (which will the shape of the right half of a
//   Gaussian distribution).
void DownsampleBayerHq(const RawReadView& src, const PlanarWriteViewU16* dst);

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_PROC_RESAMPLE_H_
