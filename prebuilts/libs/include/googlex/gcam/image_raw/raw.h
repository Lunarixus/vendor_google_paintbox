#ifndef GOOGLEX_GCAM_IMAGE_RAW_RAW_H_
#define GOOGLEX_GCAM_IMAGE_RAW_RAW_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <cstddef>
#include <cstdint>

#include "googlex/gcam/image/t_image.h"
#include "googlex/gcam/image_raw/packed_raw10.h"
#include "googlex/gcam/image_raw/packed_raw12.h"
#include "googlex/gcam/image_raw/raw_buffer_layout.h"

namespace gcam {

// Classes for storing a Bayer raw image:
//
//  * RawImage stores a raw image and owns its pixels.
//
//  * RawWriteView is an alias of a raw image that supports write access to the
//    pixels; it does not own its pixels.
//
//  * RawReadView is a read-only alias of a raw image; it does not own its
//    pixels.
//
// For more information on images and views, see t_image.h.
//
// There are three possible internal storage types for raw images:
//   1. Packed RAW10 image
//        http://developer.android.com/reference/android/graphics/ImageFormat.html#RAW10
//        //NOLINT
//        The pixel values are 10-bit.  The width must be a multiple of 4, and
//        the height must be even.  The row stride (in bytes) must be >= the
//        width (in pixels) * 5 / 4.  For more details, see packed_raw10.h.
//   2. Packed RAW12 image
//        http://developer.android.com/reference/android/graphics/ImageFormat.html#RAW12
//        //NOLINT
//        The pixel values are 12-bit.  The width must be a multiple of 4, and
//        the height must be even.  The row stride (in bytes) must be >= the
//        width (in pixels) * 3 / 2.  For more details, see packed_raw12.h.
//   3. Unpacked 16-bit raw image
//        https://developer.android.com/reference/android/graphics/ImageFormat.html#RAW_SENSOR
//        //NOLINT
//        The width and height must be even.  The pixel values may occupy from
//        1-16 bits, but are stored in 16 bits, little-endian.  Row padding is
//        acceptable.
//
// TODO(hasinoff): Consider adding support for planar raw by adding a new
//   PlanarImageU16 variant. This would bring the RawImage hierarchy into better
//   correspondence with RawBufferLayout.
//
// At most one of these variants will be non-null.

// Read-only view of a packed or unpacked Bayer raw image.
class RawReadView {
 public:
  RawReadView() = default;
  RawReadView(const RawReadView& other) = default;
  RawReadView(int width, int height, size_t row_padding, RawBufferLayout layout,
              uint8_t* data);
  virtual ~RawReadView() = default;

  // Test whether all image variants are null.
  bool operator==(std::nullptr_t) const;
  bool operator!=(std::nullptr_t) const;
  explicit inline operator bool() const { return *this != nullptr; }

  // Null out all image variants.
  virtual RawReadView& operator=(std::nullptr_t);

  int width() const;
  int height() const;

  const InterleavedReadViewU16& unpacked_read_view() const {
    return unpacked_read_view_;
  }
  const PackedReadViewRaw10& packed10_read_view() const {
    return packed10_read_view_;
  }
  const PackedReadViewRaw12& packed12_read_view() const {
    return packed12_read_view_;
  }

  // Crop the image view to the requested rectangle, or as close as possible,
  // using pointer artihmetic. The operation is fast, but if the crop rectangle
  // is small compared to the original image, then the cropped image can waste a
  // lot of memory.
  //
  // If the raw image is interleaved or packed, the rectangle may be adjusted to
  // align to make the crop more efficient.
  virtual void FastCrop(int x0, int y0, int x1, int y1);

  // Returns the size of the sample array for this image. The size is measured
  // in bytes and includes padding.
  size_t sample_array_size() const;

  inline int BitsPerPixel() const {
    if (unpacked_read_view_) {
      return 16;
    } else if (packed10_read_view_) {
      return 10;
    } else if (packed12_read_view_) {
      return 12;
    } else {
      return 0;
    }
  }

 protected:
  InterleavedReadViewU16 unpacked_read_view_;
  PackedReadViewRaw10 packed10_read_view_;
  PackedReadViewRaw12 packed12_read_view_;
};

// Write view of a packed or unpacked Bayer raw image.
class RawWriteView : public RawReadView {
 public:
  RawWriteView() = default;
  RawWriteView(const RawWriteView& other) = default;
  RawWriteView(int width, int height, size_t row_padding,
               RawBufferLayout layout, uint8_t* data);
  ~RawWriteView() override = default;

  // Null out all image variants.
  RawWriteView& operator=(std::nullptr_t) override;

  const InterleavedWriteViewU16& unpacked_write_view() const {
    return unpacked_write_view_;
  }
  const PackedReadWriteViewRaw10& packed10_write_view() const {
    return packed10_write_view_;
  }
  const PackedReadWriteViewRaw12& packed12_write_view() const {
    return packed12_write_view_;
  }

  void FastCrop(int x0, int y0, int x1, int y1) override;

 protected:
  InterleavedWriteViewU16 unpacked_write_view_;
  PackedReadWriteViewRaw10 packed10_write_view_;
  PackedReadWriteViewRaw12 packed12_write_view_;
};

// Packed or unpacked Bayer raw image.
class RawImage : public RawWriteView {
 public:
  RawImage() = default;
  RawImage& operator=(RawImage&& other);
  RawImage(RawImage&& other);
  RawImage(const RawImage& other) = delete;
  RawImage& operator=(const RawImage& other) = delete;
  ~RawImage() override = default;

  // Move constructors relinquishing ownership to the RawImage.
  explicit RawImage(InterleavedImageU16&& other);
  explicit RawImage(PackedImageRaw10&& other);
  explicit RawImage(PackedImageRaw12&& other);

  // Null out all image variants.
  RawImage& operator=(std::nullptr_t) override;

  void FastCrop(int x0, int y0, int x1, int y1) override;

  const InterleavedImageU16& unpacked_image() const { return unpacked_image_; }
  const PackedImageRaw10& packed10_image() const { return packed10_image_; }
  const PackedImageRaw12& packed12_image() const { return packed12_image_; }

 private:
  InterleavedImageU16 unpacked_image_;
  PackedImageRaw10 packed10_image_;
  PackedImageRaw12 packed12_image_;
};

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_RAW_RAW_H_
