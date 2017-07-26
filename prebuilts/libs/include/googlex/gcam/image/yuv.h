#ifndef GOOGLEX_GCAM_IMAGE_YUV_H_
#define GOOGLEX_GCAM_IMAGE_YUV_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "googlex/gcam/image/pixel_format.h"
#include "googlex/gcam/image/t_image.h"

namespace gcam {

enum class YuvFormat {
  kInvalid = 0,

  // Corresponds to an YuvImage with NV12 format.
  // The luma channel is stored in a full-resolution plane, and the
  // 2x2-subsampled chroma channels (U and V) are stored, interleaved, in
  // a second plane, in the NV12 ordering (UVUV...).
  kNv12,

  // Corresponds to an YuvImage with NV21 format.
  // The luma channel is stored in a full-resolution plane, and the
  // 2x2-subsampled chroma channels (V and U) are stored, interleaved, in
  // a second plane, in the NV21 ordering (VUVU...).
  kNv21
};

const char* ToText(YuvFormat format);
YuvFormat TextToYuvFormat(const char* text);

// Returns whether the two formats correspond to YUV images that have their UV
// order swapped. It is only valid to call this function on formats with
// interleaved UV (YuvFormat::kNv12 and YuvFormat::kNv21).
bool YuvFormatsHaveUvSwapped(YuvFormat format1, YuvFormat format2);

inline YuvFormat ToYuvFormat(GcamPixelFormat format) {
  if (format == GcamPixelFormat::kNv12) {
    return YuvFormat::kNv12;
  } else if (format == GcamPixelFormat::kNv21) {
    return YuvFormat::kNv21;
  } else {
    return YuvFormat::kInvalid;
  }
}

// Classes for storing YUV 8-bit semi-planar images:
//
//  * YuvImage stores a YUV image and owns its pixels.
//
//  * YuvWriteView is a read-write alias of a YUV image;
//    it does not own its pixels.
//
//  * YuvReadView is a read-only alias of a raw image;
//    it does not own its pixels.
//
// For more information on images and views see the comments
// at the beginning of lib_image/t_image.h.
//
// A YuvImage contains two sub-images.  One of the sub-images stores a
// full-resolution luma channel; the other stores two half-resolution
// chroma channels.  A yuv_format flag determines which of the two chroma
// channels is U and which is V.
//

// Read-only view of an 8-bit semi-planar YUV image.
class YuvReadView {
 public:
  // Copies a read-only YUV image view, creating a new read-only view that
  // shares its samples with the original view.
  YuvReadView(const YuvReadView& other) = default;

  // This constructor assumes that the input data is interleaved.
  YuvReadView(int luma_width, int luma_height, int luma_channels,
              int luma_row_stride, uint8_t* luma_base_pointer,
              int chroma_width, int chroma_height, int chroma_channels,
              int chroma_row_stride, uint8_t* chroma_base_pointer,
              YuvFormat yuv_format);

  YuvReadView() = default;
  virtual ~YuvReadView() = default;
  YuvReadView& operator=(const YuvReadView &other) = default;
  bool operator==(std::nullptr_t) const;
  inline bool operator!=(std::nullptr_t) const {
    return !(*this == nullptr);
  }
  virtual YuvReadView& operator=(std::nullptr_t);
  explicit operator bool() const { return !(*this == nullptr); }
  virtual void FastCrop(int x0, int y0, int x1, int y1);

  // Access to the luma and chroma components of this image view.
  // The luma view contains a single channel; the chroma view contains
  // two interleaved channels.
  const InterleavedReadViewU8& luma_read_view() const {
    return luma_read_view_;
  }
  const InterleavedReadViewU8& chroma_read_view() const {
    return chroma_read_view_;
  }

  // The YUV format of this YUV image view.
  YuvFormat yuv_format() const { return yuv_format_; }

  // Width and height of this YUV image view.
  int width() const { return luma_read_view().width(); }
  int height() const { return luma_read_view().height(); }

  // y_at() returns a reference to the byte (Y value) at a pixel
  //   location in the luma image.
  const uint8_t& y_at(int x, int y) const {
    assert(x >= 0);
    assert(y >= 0);
    assert(x < width());
    assert(y < height());
    return luma_read_view().at(x, y, 0);
  }

  // uv_at() returns a reference to the U or V value
  //   at a pixel location *in the half-res UV image*.
  // The byte order will be UV if yuv_format is YuvFormat::kNv12, or VU if it is
  //   YuvFormat::kNv21.
  // (Note that sometimes, when doing RGB<>YUV conversions if the YUV
  //   container is NV21, then it is often simpler to swap appropriate
  //   elements of the matrix, and then let your [high-performance] code
  //   assume that the byte order is always UV.
  const uint8_t& uv_at(int x, int y, int channel) const {
    assert(x >= 0);
    assert(y >= 0);
    assert(x < width());
    assert(y < height());
    return chroma_read_view().at(x >> 1, y >> 1, channel);
  }

  // Returns 'true' if the memory for the luma and chroma images was allocated
  // in a single chunk, was never "fast-cropped", and is as lean as possible
  // (i.e., no wasted padding bytes at the end of any rows).
  bool IsOneLeanChunk() const {
    return (chroma_read_view().base_pointer() ==
            luma_read_view().base_pointer() + width() * height() &&
            static_cast<int>(luma_read_view().y_stride()) == width() &&
            static_cast<int>(chroma_read_view().y_stride()) ==
            chroma_read_view().width() * 2);
  }

 protected:
  YuvReadView(const InterleavedReadViewU8& luma,
              const InterleavedReadViewU8& chroma,
              YuvFormat yuv_format)
    : luma_read_view_(luma), chroma_read_view_(chroma),
      yuv_format_(yuv_format) {}

  // A full-sized, 1-channel luma image.
  // And a half-sized, 2-channel chroma image.
  InterleavedReadViewU8 luma_read_view_;
  InterleavedReadViewU8 chroma_read_view_;

  // Gcam uses NV12 internally, which is a full-resolution Y
  // plane, then an interleaved, half-res UV plane.  NV21 is
  // the same thing, except the pixel order in the UV plane
  // is VUVU... rather than UVUV...  Therefore, if the frames
  // you pass into Gcam are NV21, be sure to set this to YuvFormat::kNv21.
  YuvFormat yuv_format_;  // Default: YuvFormat::kNv12.
};

// Read-write view of an 8-bit semi-planar YUV image.
class YuvWriteView : public YuvReadView {
 public:
  YuvWriteView() = default;

  ~YuvWriteView() override = default;

  YuvWriteView(const YuvWriteView& other) = default;

  // Copies a read-write YUV image view, creating a new read-write view that
  // shares its samples with the original view.
  YuvWriteView& operator=(const YuvWriteView& other) = default;

  // This constructor assumes that the input data is interleaved.
  YuvWriteView(int luma_width, int luma_height, int luma_channels,
               int luma_row_stride, uint8_t* luma_base_pointer,
               int chroma_width, int chroma_height, int chroma_channels,
               int chroma_row_stride, uint8_t* chroma_base_pointer,
               YuvFormat yuv_format);

  // Access to the luma and chroma components of this image view.
  // The luma view contains a single channel; the chroma view contains
  // two interleaved channels.
  const InterleavedWriteViewU8& luma_write_view() const {
    return luma_write_view_;
  }

  const InterleavedWriteViewU8& chroma_write_view() const {
    return chroma_write_view_;
  }

  // at() returns a reference to the byte (Y value) at a pixel
  //   location in the luma image.
  uint8_t& y_at(int x, int y) const {
    assert(x >= 0);
    assert(y >= 0);
    assert(x < width());
    assert(y < height());
    return luma_write_view().at(x, y, 0);
  }

  // uv_at() returns a reference to the U or V value
  //   at a pixel location *in the half-res UV image*.
  // The byte order will be UV if yuv_format is YuvFormat::kNv12, or VU if it is
  //   YuvFormat::kNv21.
  // (Note that sometimes, when doing RGB<>YUV conversions if the YUV
  //   container is NV21, then it is often simpler to swap appropriate
  //   elements of the matrix, and then let your [high-performance] code
  //   assume that the byte order is always UV.
  uint8_t& uv_at(int x, int y, int channel) const {
    assert(x >= 0);
    assert(y >= 0);
    assert(x < width());
    assert(y < height());
    return chroma_write_view().at(x >> 1, y >> 1, channel);
  }

  // Change the YUV format of this image.
  void SetYuvFormat(YuvFormat yuv_format) { yuv_format_ = yuv_format; }
  YuvWriteView& operator=(std::nullptr_t) override;
  void FastCrop(int x0, int y0, int x1, int y1) override;

 protected:
  YuvWriteView(const InterleavedWriteViewU8& luma,
               const InterleavedWriteViewU8& chroma,
               YuvFormat yuv_format)
    : YuvReadView(luma, chroma, yuv_format), luma_write_view_(luma),
      chroma_write_view_(chroma) {}

  InterleavedWriteViewU8 luma_write_view_;
  InterleavedWriteViewU8 chroma_write_view_;
};

// An 8-bit semi-planar YUV image with a full-resolution luma channel
// and two half-resolution chroma channels.
class YuvImage : public YuvWriteView {
 public:
  ~YuvImage() override = default;
  YuvImage() = default;

  // Creates a new YuvImage of the desired size, allocating all image memory in
  // two chunks.
  YuvImage(int w, int h, YuvFormat yuv_format,
           TImageSampleAllocator* custom_allocator =
               TImageDefaultSampleAllocator());

  YuvImage(InterleavedImageU8&& luma, InterleavedImageU8&& chroma,
           YuvFormat yuv_format);

  YuvImage(const YuvImage& other) = delete;
  YuvImage& operator=(const YuvImage& other) = delete;

  YuvImage(YuvImage&& other);
  YuvImage& operator=(YuvImage&& other);

  // Access to the luma and chroma components of this image view.
  // The luma image contains a single channel; the chroma image contains
  // two interleaved channels.
  const InterleavedImageU8& luma_image() const {
    return luma_image_;
  }

  const InterleavedImageU8& chroma_image() const {
    return chroma_image_;
  }

  YuvImage& operator=(std::nullptr_t) override;
  void FastCrop(int x0, int y0, int x1, int y1) override;

 protected:
  // The luma and chroma images.
  InterleavedImageU8 luma_image_;
  InterleavedImageU8 chroma_image_;
};

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_YUV_H_
