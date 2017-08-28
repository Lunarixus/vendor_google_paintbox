#ifndef GOOGLEX_GCAM_IMAGE_YUV_UTILS_H_
#define GOOGLEX_GCAM_IMAGE_YUV_UTILS_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include "googlex/gcam/base/pixel_rect.h"
#include "googlex/gcam/image/t_image.h"
#include "googlex/gcam/image/yuv.h"

namespace gcam {

class ColorMatrix;
struct Context;

bool IsGood(const YuvReadView& frame);

// Duplicates the image data, preserving the row strides (amount of padding)
// of the original image.
//
// The optional Context parameter is for internal use only and is used for
// multi-threading copies. (Pass the default value of nullptr for a
// single-threaded copy.)
//
// The new image is allocated using 'allocator'.
YuvImage MakeCopy(
    const YuvReadView& yuv, const Context* gcam_context = nullptr,
    TImageSampleAllocator* allocator = TImageDefaultSampleAllocator());

// Notes about the "Yuv888" type and conversions to/from it:
//  - Yuv888 images are currently stored as a PlanarImageU8.
//  - Yuv888 images store the image with all 3 channels (Y, U, V) at each
//      pixel; i.e. there is no chroma downsampling.  Each color channel is
//      stored in a separate plane in memory.
//  - Yuv888 images always use an NV12 color plane order.  (Plane 0 is Y,
//      plane 1 is U, plane 2 is V).
//  - TODO(geiss): Create a Yuv888ni type (a thin wrapper around
//      PlanarImageU8), with a YuvFormat member, so that we can properly
//      track NV12-ness.  (Note: At that point, we can drop the restriction
//      of these images all being NV12, because calling SwapUV() on them would
//      just be a quick pointer swap.)

// TODO(hasinoff): Consider automatically handling YuvImage's whose YuvFormat
//   is swapped from yuv2rgb.input_type (or rgb2yuv.output_type), resolving
//   the discrepancy in favor of the YuvImage.

// Functions to convert between RGB and YUV.
//
// The versions without a ColorMatrix parameter use the standard YUV-to-RGB
// matrix (or its inverse), adjusted based on the YuvFormat to take channel
// swapping into account.
//
// ALL CONVERSIONS USE ROUND-TO-NEAREST.

// YUV to RGB conversion.
//
// The versions without a ColorMatrix use the standard one, specified by
// ColorMatrix::GetStandardYuvToRgb().
//
// On failure, a null InterleavedImageU8 or false will be returned.
InterleavedImageU8 YuvToRgb(const YuvReadView& yuv);
InterleavedImageU8 YuvToRgb(const ColorMatrix& yuv2rgb, const YuvReadView& yuv);
bool YuvToRgb(const YuvReadView& yuv, InterleavedWriteViewU8* rgb);
bool YuvToRgb(const ColorMatrix& yuv2rgb, const YuvReadView& yuv,
              InterleavedWriteViewU8* rgb);

// RGB to YUV conversion.
//
// The versions without a ColorMatrix use the standard one, specified by
// ColorMatrix::GetStandardRgbToYuv().
//
// These functions require the RGB image to have even width and height, so that
// a valid YUV image can be produced. On failure, a null YuvImage or false will
// be returned.
YuvImage RgbToYuv(const InterleavedReadViewU8& rgb);
YuvImage RgbToYuv(const ColorMatrix& rgb2yuv, const InterleavedReadViewU8& rgb);
bool RgbToYuv(const InterleavedReadViewU8& rgb, YuvWriteView* yuv);
bool RgbToYuv(const ColorMatrix& rgb2yuv, const InterleavedReadViewU8& rgb,
              YuvWriteView* yuv);

// This version will produce a YUV version of the image, but potentially
// cropped by up to 1 pixel on either axis, if necessary, so that a valid
// (even-width and -height) YUV image can be produced.
YuvImage RgbToPossiblyCroppedYuv(const InterleavedReadViewU8& rgb);
YuvImage RgbToPossiblyCroppedYuv(const ColorMatrix& rgb2yuv,
                                 const InterleavedReadViewU8& rgb);

InterleavedImageU8 YuvToHalfsizeRgb(const YuvReadView& yuv);
InterleavedImageU8 YuvToHalfsizeRgb(const ColorMatrix& yuv2rgb,
                                    const YuvReadView& yuv);

// Upsamples chroma channels (1 pixel -> 2x2 pixels) using nearest-neighbor
//   replication.
// Note: The returned yuv888 image will have an NV12 color plane order, even
//   if the input image is NV21.
PlanarImageU8 Yuv844ToYuv888ni(const YuvReadView& yuv844);

// Downsamples chroma channels (2x2 pixels -> 1 pixel) by averaging.
// The returned YuvImage will be NV12.
YuvImage Yuv888niToYuv844(const PlanarReadViewU8& yuv888);

InterleavedImageU8 Yuv888niToRgb(const PlanarReadViewU8& yuv888,
                                 const Context& gcam_context);

// Crop the image to the requested rectangle, or as close as possible, using
// pointer artihmetic. The operation is fast, but if the crop rectangle is
// small compared to the original image, then the cropped image can waste a
// lot of memory.
//
// Because the chroma channel of YUV is downsampled by a factor of 2, the crop
// is constrained to be a multiple of 2x2 pixels, and aligned accordingly.
//
// FastCrop() will adjust the rectangle, if necessary, to satisfy these
// constaints.
//
bool FastCrop(YuvReadView* src, const PixelRect& rect);
bool FastCrop(YuvReadView* src, const NormalizedRect& crop);

// Crops the image to a centered rectangle with width w2 and height h2, or as
// close as possible, using pointer arithmetic.
//
// Because the chroma channel of YUV is downsampled by a factor of 2, the crop
// is constrained to be a multiple of 2x2 pixels, and aligned accordingly.
//
// FastCropCentered() will adjust the rectangle, if necessary, to satisfy these
// constaints.
//
bool FastCropCentered(YuvReadView* src, int w2, int h2);

void SwapUV(YuvWriteView* yuv);

// Infers image dimensions from file size.
// For use with .raw (YUV) files only - not Bayer raw files (which are .dng).
bool InferRawYuvImageDimensions(int file_size_bytes, bool is_sand, int* w_out,
                                int* h_out);

// These read/write a YuvImage as blob of bytes, without any header
// information, to/from a file.
YuvImage ReadYuvImageBlob(
    const char* filename, YuvFormat yuv_format,
    TImageSampleAllocator* custom_allocator = TImageDefaultSampleAllocator());
bool WriteYuvImageBlob(const char* filename, const YuvReadView& yuv,
                       const Context& gcam_context);

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_YUV_UTILS_H_
