#ifndef GOOGLEX_GCAM_IMAGE_PACKED_RAW10_H_
#define GOOGLEX_GCAM_IMAGE_PACKED_RAW10_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "googlex/gcam/image/t_image.h"

namespace gcam {

// Classes for storing RAW10 (packed 10-bit) raw images:
//
//  * PackedImageRaw10 stores a packed raw image and owns its pixels.
//
//  * PackedReadViewRaw10 is a read-only alias of a packed raw image;
//    it does not own its pixels.
//
// For more information on images and views see the comments at the
// beginning of lib_image/t_image.h.
//
// The RAW10 format tightly packs four horizontal 10-bit pixels into 5 bytes.
//   Therefore, the underlying (logical) image width must be a multiple of
//   4, and (due to the 2x2 Bayer pattern) the height must be even.  The
//   RAW10 format does allow for extra padding bytes after each row.
// The InterleavedImageU8 is just a storage container for the data, whose
//   "width" is actually 25% larger than the width of the image, so that each
//   row has enough bytes to actually store the data.
// This class (PackedImageRaw10) attempts to hide this complexity by offering
//   functionality to unpack a specific (Bayer) pixel, or an entire row, with
//   the caller not having to worry about the actual details of the packing.
// See the full RAW10 spec here:
//   http://developer.android.com/reference/android/graphics/ImageFormat.html#RAW10
//   //NOLINT

class PackedReadViewRaw10;

class ConstSampleIteratorPackedRaw10 {
 public:
  typedef const uint16_t SampleType;

  // We don't allow direct access to the pointer since we only unpack one row
  // at a time.
  SampleType& operator*() const { return *x_; }

  // Access to the (x, y, c) coordinates of the sample that the
  // iterator currently points to.
  int x() const { return x_ - row_.begin(); }
  int y() const { return y_; }
  int c() const { return 0; }

  // Advancing the iterator to the next sample, and testing if
  // all samples have already been visited.
  void NextSample();
  bool AtEnd() const;

  // Constructor - you shouldn't ever have to call this directly;
  // the usual way to obtain an iterator is by calling
  // PackedReadViewRaw10::sample_iterator().
  explicit ConstSampleIteratorPackedRaw10(const PackedReadViewRaw10* iterating);

  // The compiler-generated default copy constructors and assignment
  // operators work for TImageSampleIterator; we don't have to define
  // our own here.
  ConstSampleIteratorPackedRaw10(const ConstSampleIteratorPackedRaw10& i) =
      default;
  ConstSampleIteratorPackedRaw10& operator=(
      const ConstSampleIteratorPackedRaw10& i) = default;

 private:
  const PackedReadViewRaw10* iterating_;

  // Store entire unpacked during iteration.
  std::vector<uint16_t> row_;

  std::vector<uint16_t>::const_iterator x_;
  int y_;
};

// Read-only view of a packed 10-bit raw image.
class PackedReadViewRaw10 {
 public:
  PackedReadViewRaw10(const PackedReadViewRaw10& other) = default;
  virtual ~PackedReadViewRaw10() = default;

  // These return the *logical* size of the packed image, in pixels.
  // (The logical height and packed height are the same, but the
  //   widths differ; the packed width is 25% more than the logical width.)
  int width() const { return packed_read_view_.width() * 4 / 5; }
  int height() const { return packed_read_view_.height(); }
  // This exists to mirror the API of other image classes (so it works with
  // template functions).
  int num_channels() const { return 1; }

  // Returns the size of the sample array for this image. The size is measured
  // in bytes and includes padding.
  inline size_t sample_array_size() const {
    return packed_read_view_.sample_array_size();
  }

  inline bool operator==(std::nullptr_t) const {
    return packed_read_view_ == nullptr;
  }
  inline bool operator!=(std::nullptr_t) const {
    return packed_read_view_ != nullptr;
  }
  virtual PackedReadViewRaw10& operator=(std::nullptr_t) {
    packed_read_view_ = nullptr;
    return *this;
  }
  explicit inline operator bool() const { return *this != nullptr; }

  // Unpacks a row of the packed RAW10 data.
  // The dst[] buffer should be large enough to hold width() elements.
  // The unpacked image will be single-channel and the pixel data will be
  //   in the original Bayer pattern order.
  // We unpack a row at a time for two reasons:
  //   1. Makes it easier to multi-thread an unpack, if desired.
  //   2. You can optionally unpack only the rows you need, if doing
  //      sparse sampling (e.g. for metering, sharpness measurement, etc).
  void UnpackRow(int x0, int x1, int y, uint16_t* dst) const;
  inline void UnpackRow(int y, uint16_t* dst) const {
    return UnpackRow(0, width(), y, dst);
  }

  // Unpacks the packed RAW10 data into a single-channel 16-bit image.
  // The destination image provided by the caller must be single-channel,
  //   and its dimensions must match the logical width and height of the
  //   packed image.
  void Unpack(InterleavedWriteViewU16* dst) const;

  // Samples a single Bayer pixel from the packed image, at the equivalent
  //   logical coordinates (x,y).
  // The return value will be in the range [0..1023].  (Note that it will
  //   likely include a black level offset bias.)
  inline uint16_t at(int x, int y) const {
    assert(packed_read_view());
    assert(x >= 0);
    assert(x < width());
    assert(y >= 0);
    assert(y < height());

    // Each group of 4 pixels (a,b,c,d) is packed into 5 bytes:
    //   aaaaaaaa bbbbbbbb cccccccc dddddddd ddccbbaa

    // Get the starting index for the 5-byte chunk that holds our pixel:
    int x2 = (x & ~3) + (x >> 2);

    // Fetch the 8 MSBs for the pixel of interest.
    const uint16_t byte1 = packed_read_view_.at(x2 + (x & 3), y, 0);

    // Fetch a byte containing the 2 LSBs for all 4 pixels.
    const uint8_t byte2 = packed_read_view_.at(x2 + 4, y, 0);

    // Assemble the result.
    const int shift = (x & 3) * 2;
    const uint16_t unpacked = (byte1 << 2) | ((byte2 >> shift) & 3);
    return unpacked;
  }

  // Packed raw images only have one channel, but this version of at() is here
  // so that some functions (such as BayerToRgb() in split_hdr_image.cc) can be
  // templatized over multiple image types, and can sample this image like most
  // other images - using at(x, y, ch).
  inline uint16_t at(int x, int y, int ch) const {
    assert(ch == 0);
    (void)ch;  // Suppress -Wunused-variable.
    return at(x, y);
  }

  // Create a sample iterator for this view.
  ConstSampleIteratorPackedRaw10 sample_iterator() const {
    return ConstSampleIteratorPackedRaw10(this);
  }

  // Crop the image to the requested rectangle, using pointer artihmetic.
  // The operation is fast, but if the crop rectangle is small compared to the
  // original image, then the cropped image can waste a lot of memory.
  //
  // Because of the RAW10 image type, the crop must be a multiple of 4x2 pixels,
  // and aligned accordingly.
  virtual bool FastCrop(int x0, int y0, int x1, int y1);

  const InterleavedReadViewU8& packed_read_view() const {
    return packed_read_view_;
  }

  explicit PackedReadViewRaw10(const InterleavedReadViewU8& packed)
      : packed_read_view_(packed) {}

  PackedReadViewRaw10() = default;

 protected:
  // Once assigned, this shouldn't change, so that we can make view and image
  // synchronized.
  InterleavedReadViewU8 packed_read_view_;
};

// Read-Write view of a packed 10-bit raw image.
class PackedReadWriteViewRaw10 : public PackedReadViewRaw10 {
 public:
  PackedReadWriteViewRaw10(const PackedReadWriteViewRaw10& other) = default;
  ~PackedReadWriteViewRaw10() override = default;

  PackedReadWriteViewRaw10& operator=(std::nullptr_t) override;

  // Pack a row of RAW16 data and write it to this image.
  // The row[] buffer should have x1 - x0 elements.
  void set_row(int x0, int x1, int y, const uint16_t* row) const;
  inline void set_row(int y, const uint16_t* row) const {
    set_row(0, width(), y, row);
  }

  const InterleavedWriteViewU8& packed_write_view() const {
    return packed_write_view_;
  }

  // Warning: This function is SLOW.
  inline void set_pixel(int x, int y, uint16_t value) const {
    assert(x >= 0);
    assert(y >= 0);
    assert(x < width());
    assert(y < height());
    assert(value <= 1023);

    // Get the starting index for the 5-byte chunk that holds our pixel:
    int x2 = (x & ~3) + (x >> 2);

    // Get a pointer to the 5-byte block that stores this pixel.
    uint8_t* ptr = packed_write_view().base_pointer() +
                   packed_write_view().y_stride() * y + x2;

    // Write the 8 MSB's.
    ptr[x & 3] = (value >> 2);

    // Reset the 2 LSB's that were there before.
    // (The 5th byte layout is: ddccbbaa.)
    const uint8_t shift = (x & 3) * 2;
    ptr[4] &= ~(3 << shift);

    // Or in the 2 new LSB's.
    ptr[4] |= (value & 3) << shift;
  }
  bool FastCrop(int x0, int y0, int x1, int y1) override;
  explicit PackedReadWriteViewRaw10(const InterleavedWriteViewU8& packed)
      : PackedReadViewRaw10(packed), packed_write_view_(packed) {}
  PackedReadWriteViewRaw10() = default;

 protected:
  InterleavedWriteViewU8 packed_write_view_;
};

// 10-bit packed raw image.
class PackedImageRaw10 : public PackedReadWriteViewRaw10 {
 public:
  PackedImageRaw10() = default;

  // Constructor will take the ownership of the input.
  explicit PackedImageRaw10(InterleavedImageU8&& packed_data);

  // Create a copy of a 16-bit raw image by packing it to the packed RAW10
  // format.
  explicit PackedImageRaw10(
      const InterleavedReadViewU16& raw16_image,
      TImageSampleAllocator* custom_allocator = TImageDefaultSampleAllocator());
  explicit PackedImageRaw10(
      const PlanarReadViewU16& raw16_image,
      TImageSampleAllocator* custom_allocator = TImageDefaultSampleAllocator());

  // Create a new empty packed RAW10 image.
  // NOTE: The optional row padding is specified in terms of the number of
  // samples in the underlying *packed* U8 image.
  PackedImageRaw10(
      int width, int height, TImageInit init = kInitUndefined,
      size_t packed_row_padding = 0,
      TImageSampleAllocator* custom_allocator = TImageDefaultSampleAllocator());

  PackedImageRaw10(const PackedImageRaw10& other) = delete;

  // Assignment is not supported.
  PackedImageRaw10& operator=(const PackedImageRaw10& other) = delete;

  PackedImageRaw10& operator=(PackedImageRaw10&& other);

  const InterleavedImageU8& packed_image() const { return packed_image_; }

  bool FastCrop(int x0, int y0, int x1, int y1) override;
  PackedImageRaw10& operator=(std::nullptr_t) override;

 private:
  InterleavedImageU8 packed_image_;
};

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_PACKED_RAW10_H_
