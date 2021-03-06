#ifndef GOOGLEX_GCAM_IMAGE_T_IMAGE_H_
#define GOOGLEX_GCAM_IMAGE_T_IMAGE_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace gcam {

// TImage -- a class template for 2D images:
//
// An "image" is a three-dimensional array of values or "samples" of type
// SampleType.  A sample is addressed by three coordinates, x, y and c.
// The set of all samples that have the same x and y coordinates is a "pixel,"
// the set of all samples that have the same c coordinate is a "channel,"
// and the set of all samples that have the same y coordinate is a "row."
//
// For an image with width w, height h and n channels, valid x, y and c
// coordinates go from 0 to w-1, 0 to h-1, and 0 to n-1 respectively.
// Attempting to access samples outside the valid coordinate range causes
// undefined behavior.
//
// For the purpose of describing operations such as crop, fill and copy,
// the pixel with x and y coordinates (0, 0) is in the top left corner of
// the image, x increases toward the right, and y increases toward the bottom.
//
// Sometimes application code wants to operate only on a rectangular part
// of an image rather than on the entire image.  In order to make iteration
// over the samples in a region more convenient, rectangular image regions
// can be created by constructing "read-only views" and "read-write views"
// of an image.  A view behaves mostly like an image, but with two
// exceptions:
//
//  - Read-only views do not allow write access to their samples.
//
//  - Views do not own their samples; the samples are owned by the original
//    image from which the views were constructed.  When the original image
//    is destroyed, its samples are deallocated, and any views that were
//    constructed from it become invalid.  Accessing the samples of an invalid
//    view leads to undefined behavior (most likely a crash).
//
// Images and views are implemented as a three-level hierarchy of class
// templates:
//
//  ReadOnlyTImageView
//    - is the base class of the hierarchy
//    - does not own its samples
//    - supports read-only access to samples
//    - supports fast cropping (without memory reallocation)
//    - supports shallow copying (copies share samples with original view)
//
//  ReadWriteTImageView
//    - is derived from ReadOnlyImageView
//    - does not own its samples
//    - supports write access to samples
//
//  TImage
//    - is derived from ReadWriteTImageView
//    - usually owns its samples
//    - supports slow cropping (with reallocation and compaction of samples)
//    - supports deep copying
//

// How are the samples laid out in memory?
enum TImageLayout {
  kPixelContiguous,     // The channels are interleaved in memory.
                        // All samples in a pixel are next to each other.
                        // There may be padding at the beginning and end
                        // of each row, before the first row, and after
                        // the last row.
                        // For example, in a three-channel image with 4
                        // by 2 pixels and some amount of padding, channels
                        // 0, 1 and 2 are interleaved like this:
                        //            ppppppppppppppp
                        //            pp012012012012p
                        //            pp012012012012p
                        //            ppppppppppppppp

  kChannelContiguous,   // Planar.
                        // All samples in a channel are contiguous, except
                        // for possible padding at the end of each row.
                        // There may be additional padding before the first
                        // row and after the last row in each channel.
                        // The channels in a three-channel 4 by 2 pixel
                        // image are stored like this:
                        //            ppppppp
                        //            pp0000p
                        //            pp0000p
                        //            ppppppp
                        //            ppppppp
                        //            pp1111p
                        //            pp1111p
                        //            ppppppp
                        //            ppppppp
                        //            pp2222p
                        //            pp2222p
                        //            ppppppp
};

// Are the samples of a newly constructed image zero-initialized?
enum TImageInit {
  kInitUndefined,       // The initial value of the samples is undefined.
  kInitZero,            // The initial value of the samples is zero.
};

// The default memory allocator for samples in an image that will be used
// when no other allocator has been specified during image construction.
//
// The default allocator that is installed at program startup allocates
// memory by calling ::operator new[], and deallocates memory by calling
// ::operator delete[].
//
// A different default memory allocator can be installed by calling the
// SetTImageDefaultSampleAllocator() function.  Changing the default
// allocator has no effect on existing images, and it does not destroy
// the previous default allocator.
class TImageSampleAllocator;

TImageSampleAllocator* TImageDefaultSampleAllocator();
void SetTImageDefaultSampleAllocator(TImageSampleAllocator* allocator);

// Helper classes that are required by images and image views.
template <typename T, TImageLayout layout> class TImageSampleIterator;
template <TImageLayout layout> class TImageStrides;

// Forward declaration.
template <typename T, TImageLayout layout>
class TImage;

// A read-only image view whose samples are of type T.
template <typename T, TImageLayout layout>
class ReadOnlyTImageView {
 public:
  // Allow templated code that uses ReadOnlyImageView to refer to the type
  // of the image samples.
  typedef T SampleType;

  // Construct a read-only image view by making a shallow copy of an
  // existing view.  The original and the new view share their samples.
  ReadOnlyTImageView(const ReadOnlyTImageView& other);

  // Construct a read-only image view and fast-crop it immediately.
  // Constructing an image view like this:
  //
  //  ReadOnlyTImageView v2(v1, x0, y0, c0, x1, y1, c1);
  //
  // is equivalent to
  //
  //  ReadOnlyTImageView v2(v1);
  //  v2.FastCrop(x0, y0, c0, x1, y1, c1);
  //
  ReadOnlyTImageView(const ReadOnlyTImageView& other, int x0, int y0, int c0,
                     int x1, int y1, int c1);

  // ReadOnlyTImageView v2(v1, x0, y0, x1, y1) is equivalent to
  //   ReadOnlyTImageView v2(v1, x0, y0, 0, x1, y1, v1.num_channels());
  ReadOnlyTImageView(const ReadOnlyTImageView& other,
                     int x0, int y0, int x1, int y1);

  ReadOnlyTImageView() = default;

  // Interprets external memory passed in via 'base_pointer' as a read-only
  // image view. The resulting view does not take ownership of the memory.
  // TODO(jiawen): Row padding calculation is cumbersome. It is more natural to
  // simply specify x, y, and c strides in bytes.
  ReadOnlyTImageView(
      int width, int height, int num_channels, SampleType* base_pointer,
      size_t row_padding);
  virtual ~ReadOnlyTImageView();

  ReadOnlyTImageView& operator=(const ReadOnlyTImageView& other);

  // Width, height, number of channels and layout of the image:
  int width() const { return strides_.width_; }
  int height() const { return strides_.height_; }
  int num_channels() const { return strides_.num_channels_; }
  static constexpr TImageLayout sample_layout() { return layout; }

  // Addressing pixels and samples:
  // The sample with coordinates (x, y, c) can be accessed by calling
  //
  //   at(x, y, c).
  //
  // The sample can also be accessed via explicit pointer arithmetic;
  // it is stored at
  //
  //   base_pointer()[x * x_stride() + y * y_stride() + c * c_stride()]
  //
  // Note that the unit of the strides is sizeof(SampleType), not bytes.
  // Also, y_stride() is not necessarily equal to width() * x_stride(),
  // and c_stride() is not necessarily equal to either 1 or height() *
  // y_stride().
  const T& at(int x, int y, int c) const;

  // Return true if the base pointer is null, false otherwise.
  inline bool operator==(std::nullptr_t) const {
    return base_pointer_ == nullptr;
  }

  // Return false if the current base pointer is null, true otherwise.
  inline bool operator!=(std::nullptr_t) const {
    return !(*this == nullptr);
  }

  // Set the view's base pointer to nullptr.
  virtual ReadOnlyTImageView& operator=(std::nullptr_t) {
    base_pointer_ = nullptr;
    strides_ = TImageStrides<layout>(0, 0, 0, 0);
    return *this;
  }

  // Overload operator bool() to return false if this view points to nullptr,
  // true otherwise.
  explicit inline operator bool() const { return *this != nullptr; }

  T* base_pointer() const { return base_pointer_; }
  size_t x_stride() const { return strides_.x_stride_; }
  size_t y_stride() const { return strides_.y_stride_; }
  size_t c_stride() const { return strides_.c_stride_; }
  size_t sizeof_sample_type() const { return sizeof(SampleType); }
  int row_padding() const { return strides_.row_padding(); }

  // sample_array_size() returns the size of the array that holds the samples
  // in this image.  The size is measured in bytes and includes padding.
  //
  size_t sample_array_size() const;

  // Sometimes you want to write an algorithm that iterates over all
  // samples in an image, but in such a way that cache hit rates are
  // maximized.  If the image is in stored kPixelContiguous order,
  // then the most cache-efficient set of nested loops looks like this:
  //
  //    ReadOnlyTImageView<...> view(...);
  //    for (int y = 0; y < view.height(); ++y) {
  //      for (int x = 0; x < view.width(); ++x) {
  //        for (int c = 0; c < view.num_channels(); ++c) {
  //          const SampleType& sample = view.at(x, y, c);
  //          ...
  //        }
  //      }
  //    }
  //
  // However, if the image is in kChannelContiguous order then the loops
  // must be nested differently for maximum efficiency:
  //
  //    for (int c = 0; c < view.num_channels(); ++c) {
  //      for (int y = 0; y < view.height(); ++y) {
  //        for (int x = 0; x < view.width(); ++x) {
  //          const SampleType& sample = view.at(x, y, c);
  //          ...
  //        }
  //      }
  //    }
  //
  // In order to avoid a lot of "if (layout == ...)" statements, code
  // that doesn't care exactly in what order samples are accessed can use
  // an iterator instead of explicit pointers and strides.  The iterator
  // automatically loops over the samples in the most cache-efficient
  // order, without checking the image layout at runtime.
  //
  // Usage example:
  //
  //    ReadOnlyTImageView<...> view(...);
  //    for (auto i = view.sample_iterator(); !i.AtEnd(); i.NextSample()) {
  //      const SampleType sample = *i;
  //      ...
  //    }
  //
  // Note that the iterator has methods for querying the x, y and c coordinates
  // of the sample that the iterator currently points to.  This is useful for
  // writing loops like the following, which implements a 2x2 pixel box blur:
  //
  //  ReadOnlyTImageView<...> big(...);
  //  TImage<...> small(big.width() / 2, big.height() / 2, big.num_channels());
  //
  //  for (auto i = small.sample_iterator(); !i.AtEnd(); i.NextSample()) {
  //    *i = (big.at(i.x(),     i.y(),     i.c()) +
  //          big.at(i.x() + 1, i.y(),     i.c()) +
  //          big.at(i.x(),     i.y() + 1, i.c()) +
  //          big.at(i.x() + 1, i.y() + 1, i.c())) / 4;
  //  }
  //
  typedef TImageSampleIterator<const T, layout> ConstSampleIterator;

  // Iterate over all channels
  ConstSampleIterator sample_iterator() const;

  // Iterate only over channel c.
  ConstSampleIterator sample_iterator(int c) const;

  // Cropping an image view:
  //
  // FastCrop(x0, y0, c0, x1, y1, c1) crops an image view to a rectangle whose
  // upper left and lower right corners are at (x0, y0, c0) and (x1, y1, c1)
  // respectively--c0 and c1 specify the range of color channels to preserve in
  // the cropped result.  The top left corner is inclusive, while the bottom
  // right corner is exclusive.
  //
  // If the crop rectangle extends outside the bounds of the original image
  // view, then the crop rectangle itself is cropped before the image view
  // is modified.  Cropping can only remove pixels from an image view; it
  // cannot add pixels.
  //
  // Fast cropping is achieved by resetting the base pointer and the width
  // and height of the image view so that pixels outside the crop rectangle
  // turn into padding.
  //
  // Fast cropping does not affect the width and height of other images or
  // image views that share their sample memory with this image view.  A pixel
  // that is considered to be inside one view may count as padding for another
  // view.
  void FastCrop(int x0, int y0, int c0, int x1, int y1, int c1);

  // FastCrop(x0, y0, x1, y1) is equivalent to
  //   FastCrop(x0, y0, 0, x1, y1, num_channels());
  void FastCrop(int x0, int y0, int x1, int y1);

  // Tests if the samples for this image view form a single compact block
  // in memory, without padding between rows or channels.
  bool SamplesAreCompact() const;

 protected:
  ReadOnlyTImageView(const TImageStrides<layout>& strides,
                     SampleType* base_pointer);

  TImageStrides<layout> strides_ = TImageStrides<layout>(0, 0, 0, 0);
  SampleType* base_pointer_ = nullptr;
};

// A read-write image view whose samples are of type T.
template <typename T, TImageLayout layout>
class ReadWriteTImageView : public ReadOnlyTImageView<T, layout> {
 public:
  typedef ReadOnlyTImageView<T, layout> ReadOnlyView;
  typedef T SampleType;

  // Construct a read-write image view by making a shallow copy of an
  // existing view.  The original and the new view share their samples.
  ReadWriteTImageView(const ReadWriteTImageView& other);

  // Construct a read-write image view and fast-crop it immediately.
  // Constructing an image view like this:
  //
  //  ReadWriteTImageView v2(v1, x0, y0, c0, x1, y1, c1);
  //
  // is equivalent to
  //
  //  ReadWriteTImageView v2(v1);
  //  v2.FastCrop(x0, y0, c0, x1, y1, c1);
  //
  ReadWriteTImageView(const ReadWriteTImageView& other, int x0, int y0, int c0,
                      int x1, int y1, int c1);

  // ReadWriteTImageView v2(v1, x0, y0, x1, y1) is equivalent to
  //   ReadWriteTImageView v2(v1, x0, y0, 0, x1, y1, v1.num_channels());
  ReadWriteTImageView(const ReadWriteTImageView& other,
                      int x0, int y0, int x1, int y1);

  ReadWriteTImageView() : ReadOnlyView() {}

  ReadWriteTImageView(int width, int height, int num_channels,
                      SampleType* base_pointer, size_t row_padding);
  ~ReadWriteTImageView() override;

  ReadWriteTImageView& operator=(const ReadWriteTImageView& other);

  // Width, height, number of channels and layout of the image:
  using ReadOnlyView::width;
  using ReadOnlyView::height;
  using ReadOnlyView::num_channels;
  using ReadOnlyView::sample_layout;
  using ReadOnlyView::sizeof_sample_type;

  using ReadOnlyView::operator==;
  using ReadOnlyView::operator=;

  // Addressing pixels and samples works the same way as for read-only
  // views, but ReadWriteTImageView additionally allows write in addition
  // to read access.
  T& at(int x, int y, int c) const;

  T* base_pointer() const { return ReadOnlyView::base_pointer_; }

  using ReadOnlyView::x_stride;
  using ReadOnlyView::y_stride;
  using ReadOnlyView::c_stride;
  using ReadOnlyView::row_padding;

  typedef TImageSampleIterator<const T, layout> ConstSampleIterator;
  typedef TImageSampleIterator<T, layout> SampleIterator;

  SampleIterator sample_iterator() const;
  SampleIterator sample_iterator(int c) const;

  // Cropping an image view.
  using ReadOnlyView::FastCrop;

  // Filling and copying pixels:
  //
  // Fill(v, x0, y0, x1, y1) copies the value v into all samples inside the
  // rectangle whose upper left and lower right corners are at (x0, y0) and
  // (x1, y1) respectively.  The top left corner is inclusive, while the
  // bottom right corner is exclusive.  If the rectangle extends beyond the
  // edges of the image view, then it is cropped before it is filled.
  //
  // Fill(v) is equivalent to
  //    Fill(v, 0, 0, width(), height());
  //
  // CopyFrom(src, x0, y0, x1, y1, x, y) copies pixels from a source rectangle
  // to a destination rectangle.  The source rectangle is located in image view
  // src, and its top left and bottom right corners are are at (x0, y0) and
  // (x1, y1) respectively.  The top left corner of the destination rectangle
  // is located in this image view, and its top left corner is at (x, y).
  // The top left corner of the source rectangle is inclusive, while the
  // bottom right corner is exclusive.  If the source rectangle extends
  // beyond the edges of the source image view, or the destination rectangle
  // extends beyond the edges of this image view, then both rectangles are
  // cropped before any pixels are copied.  If the source and destination
  // image views do not have the same number of channels, then only channels
  // c through min(source.num_channels(), this->num_channels()) - 1 are copied.
  //
  // CopyFrom(src) is equivalent to
  //    CopyFrom(src, 0, 0, src.width(), src.height(), 0, 0);
  //
  // TODO(jiawen): Add Fill() and CopyFrom() flavors that accept a channel
  // range.
  void Fill(const SampleType v, int x0, int y0, int x1, int y1) const;
  void Fill(const SampleType v) const;

  // TODO(jiawen): Replace CopyFrom with a free function that accepts a
  // ReadOnlyView as input and a writeview as output. There are already too many
  // overlapping functions that do the same thing.

  // This does copy with a cropped region.
  template <typename SourceType> void CopyFrom(const SourceType& source,
                                               int x0, int y0, int x1, int y1,
                                               int x, int y) const;
  template <typename SourceType> void CopyFrom(const SourceType& source) const;

  // Tests if the sample memory for this image view includes padding.
  using ReadOnlyView::SamplesAreCompact;

  using ReadOnlyView::sample_array_size;

 protected:
  ReadWriteTImageView(const TImageStrides<layout>& strides,
                      SampleType* base_pointer);

  using ReadOnlyView::strides_;
  using ReadOnlyView::base_pointer_;
};

// An image whose samples are of type T.
template <typename T, TImageLayout layout>
class TImage : public ReadWriteTImageView<T, layout> {
 public:
  typedef ReadOnlyTImageView<T, layout> ReadOnlyView;
  typedef ReadWriteTImageView<T, layout> ReadWriteView;
  typedef T SampleType;

  // Constructs a 'null' image.
  TImage() = default;

  // Constructs a new image with the specified width, height, number of
  // channels, memory layout and initial sample values.
  //
  // Rows are padded with row_padding samples (not bytes) at the end. This can
  // be useful if the caller intends to initialize the sample array where the
  // start of each row must fall on a particular alignment.
  //
  // The provided pointer to a memory allocator is copied into the new
  // image object, and memory for the samples is acquired by calling the
  // allocator's Allocate() function. When the memory for the samples is
  // no longer needed, it is freed by calling the allocator's Deallocate()
  // function.
  //
  // The memory allocator cannot be nullptr and must persist until all TImages
  // that use it have been destroyed.
  TImage(int width, int height, int num_channels,
         TImageInit init = kInitUndefined, size_t row_padding = 0,
         TImageSampleAllocator* allocator = TImageDefaultSampleAllocator());

  // Constructs a TImage by taking ownership of an existing buffer
  // 'base_pointer' and interpreting it as an image with the given extents and
  // row padding.
  //
  // 'base_pointer' must have been allocated with 'allocator' and will be
  // freed by calling 'allocator->Deallocate()' when memory is no longer needed.
  //
  // The memory allocator cannot be nullptr and must persist until all TImages
  // that use it have been destroyed.
  TImage(int width, int height, int num_channels, size_t row_padding,
         SampleType* base_pointer, TImageSampleAllocator* allocator);

  // Constructs a new image by deep copying an existing image, including its
  // samples and any padding present. Sample memory is allocated using the same
  // allocator as 'other'.
  TImage(const TImage& other);
  TImage& operator=(const TImage& other);

  // The move constructor constructs a new image by moving the contents of an
  // existing image. After a move, 'other' will be a 'null' image.
  TImage(TImage&& other);
  TImage& operator=(TImage&& other);
  ~TImage() override;

  // Width, height, number of channels and layout of the image.
  using ReadOnlyView::width;
  using ReadOnlyView::height;
  using ReadOnlyView::num_channels;
  using ReadOnlyView::sample_layout;

  using ReadOnlyView::operator==;
  using ReadOnlyView::operator=;

  // Addressing pixels and samples.
  using ReadWriteView::at;
  using ReadWriteView::base_pointer;
  using ReadOnlyView::x_stride;
  using ReadOnlyView::y_stride;
  using ReadOnlyView::c_stride;
  using ReadOnlyView::sizeof_sample_type;
  using ReadOnlyView::row_padding;
  using ReadWriteView::sample_iterator;
  using ReadOnlyView::sample_array_size;

  using ReadOnlyView::FastCrop;

  // Filling and copying pixels.
  using ReadWriteView::Fill;
  using ReadWriteView::CopyFrom;

  // Set the image to a null image. This will release the underlying image data.
  TImage& operator=(std::nullptr_t) override;

  // Tests if the sample memory for this image view includes padding.
  using ReadOnlyView::SamplesAreCompact;

  // Access to the memory allocator for the samples.
  TImageSampleAllocator* allocator() const { return allocator_; }

 private:
  using ReadOnlyView::strides_;
  using ReadOnlyView::base_pointer_;

  T* AllocateMemory(size_t num_samples) const;
  void ReleaseMemory();

  SampleType* memory_ = nullptr;  // Owned.
  // 'allocator_' cannot be nullptr.
  TImageSampleAllocator* allocator_ = TImageDefaultSampleAllocator();
};

// An iterator for looping over the samples in an image view in the most
// cache-efficient manner: the loops from outermost to innermost are y, x, c
// for kPixelContiguous layout, or c, y, x for kChannelContiguous layout.
template <TImageLayout layout> struct TImageIteratorLoopCounters;

template <typename T, TImageLayout layout>
class TImageSampleIterator {
 public:
  typedef T SampleType;

  // Access to the sample that the iterator currently points to.
  SampleType* pointer() const;
  SampleType* operator->() const { return pointer(); }
  SampleType& operator*() const { return *pointer(); }

  // Access to the (x, y, c) coordinates of the sample that the
  // iterator currently points to.
  int x() const { return loop_counters_.x(); }
  int y() const { return loop_counters_.y(); }
  int c() const { return loop_counters_.c(); }

  // Advancing the iterator to the next sample, and testing if
  // all samples have already been visited.
  void NextSample();
  bool AtEnd() const;

  // Constructor - you shouldn't ever have to call this directly;
  // the usual way to obtain an iterator is by calling
  // ReadOnlyTImageView<...>::sample_iterator() or
  // ReadOnlyTImageView<...>::sample_iterator().
  TImageSampleIterator(const TImageStrides<layout>& strides,
                       T* base_pointer, int c);

  // The compiler-generated default copy constructors and assignment
  // operators work for TImageSampleIterator; we don't have to define
  // our own here.
  TImageSampleIterator(const TImageSampleIterator& i) = default;
  TImageSampleIterator& operator=(const TImageSampleIterator& i) = default;

 private:
  // The iterator maintains two sets of outer/middle/inner loop counters,
  // one using integer sample coordinates, and one using pointers.  See the
  // definiton of NextSample() for an explanation.
  TImageIteratorLoopCounters<layout> loop_counters_;
  const size_t outer_stride_;
  const size_t middle_stride_;
  const size_t inner_stride_;
  T* outer_pointer_;
  T* middle_pointer_;
  T* inner_pointer_;
  const size_t middle_length_;
  const size_t inner_length_;
  T* outer_end_;
  T* middle_end_;
  T* inner_end_;
};

// A memory allocator for the samples in an image.  Code that requires
// custom allocator classes should derive them from this base class.
class TImageSampleAllocator {
 public:
  TImageSampleAllocator() {}
  virtual ~TImageSampleAllocator() {}

  TImageSampleAllocator(const TImageSampleAllocator& other) = delete;
  TImageSampleAllocator& operator=(const TImageSampleAllocator& other) = delete;

  // Allocate(n) allocates a contiguous block of memory with a size of
  // at least n bytes, aligned such that a sample can be stored right
  // at the start of the block.
  virtual void* Allocate(size_t num_bytes) = 0;

  // Deallocate(b, n) frees a block, b, with a size of n bytes, that was
  // previously returned by a call to Allocate().
  virtual void Deallocate(void* memory, size_t num_bytes) = 0;
};

// An image sample memory allocator based on new and delete.
class TImageNewDeleteSampleAllocator : public TImageSampleAllocator {
 public:
  TImageNewDeleteSampleAllocator() {}

  void* Allocate(size_t num_bytes) override {
    return new char[num_bytes];
  }
  void Deallocate(void* memory, size_t /* num_bytes */) override {
    delete[] static_cast<char*>(memory);
  }
};

// Useful typedefs
typedef ReadOnlyTImageView<uint8_t, kPixelContiguous>
    InterleavedReadViewU8;
typedef ReadWriteTImageView<uint8_t, kPixelContiguous>
    InterleavedWriteViewU8;
typedef TImage<uint8_t, kPixelContiguous>
    InterleavedImageU8;

typedef ReadOnlyTImageView<uint8_t, kChannelContiguous>
    PlanarReadViewU8;
typedef ReadWriteTImageView<uint8_t, kChannelContiguous>
    PlanarWriteViewU8;
typedef TImage<uint8_t, kChannelContiguous>
    PlanarImageU8;

typedef ReadOnlyTImageView<uint16_t, kPixelContiguous>
    InterleavedReadViewU16;
typedef ReadWriteTImageView<uint16_t, kPixelContiguous>
    InterleavedWriteViewU16;
typedef TImage<uint16_t, kPixelContiguous>
    InterleavedImageU16;

typedef ReadOnlyTImageView<uint16_t, kChannelContiguous>
    PlanarReadViewU16;
typedef ReadWriteTImageView<uint16_t, kChannelContiguous>
    PlanarWriteViewU16;
typedef TImage<uint16_t, kChannelContiguous>
    PlanarImageU16;

typedef ReadOnlyTImageView<int16_t, kPixelContiguous>
    InterleavedReadViewS16;
typedef ReadWriteTImageView<int16_t, kPixelContiguous>
    InterleavedWriteViewS16;
typedef TImage<int16_t, kPixelContiguous>
    InterleavedImageS16;

typedef ReadOnlyTImageView<int16_t, kChannelContiguous>
    PlanarReadViewS16;
typedef ReadWriteTImageView<int16_t, kChannelContiguous>
    PlanarWriteViewS16;
typedef TImage<int16_t, kChannelContiguous>
    PlanarImageS16;

typedef ReadOnlyTImageView<float, kPixelContiguous>
    InterleavedReadViewF;
typedef ReadWriteTImageView<float, kPixelContiguous>
    InterleavedWriteViewF;
typedef TImage<float, kPixelContiguous>
    InterleavedImageF;

typedef ReadOnlyTImageView<float, kChannelContiguous>
    PlanarReadViewF;
typedef ReadWriteTImageView<float, kChannelContiguous>
    PlanarWriteViewF;
typedef TImage<float, kChannelContiguous>
    PlanarImageF;

//-----------------------------------------------------------------------------
// Implementation of template class TImageStrides<layout>, with specializations
// for kPixelContiguous and kChannelContiguous layouts.

template <TImageLayout layout>
class TImageStrides {
 public:
  TImageStrides(int width, int height, int num_channels, int row_padding);
  size_t outer_stride() const;
  size_t middle_stride() const;
  size_t inner_stride() const;
  int outer_limit() const;
  int middle_limit() const;
  int inner_limit() const;
  int row_padding() const;  // Row padding is in samples, not bytes.

  TImageStrides(const TImageStrides& other) = default;
  TImageStrides& operator=(const TImageStrides& other) = default;

  int width_;
  int height_;
  int num_channels_;
  size_t x_stride_;
  size_t y_stride_;
  size_t c_stride_;
  size_t num_samples_;
};

template <>
class TImageStrides<kPixelContiguous> {
 public:
  TImageStrides(int width, int height, int num_channels, int row_padding)
      : width_(width),
        height_(height),
        num_channels_(num_channels),
        x_stride_(num_channels),
        y_stride_(x_stride_ * width + row_padding),
        c_stride_(1),
        num_samples_(y_stride_ * height) {}

  TImageStrides(const TImageStrides &other) = default;
  TImageStrides& operator=(const TImageStrides& other) = default;

  size_t outer_stride() const { return y_stride_; }
  size_t middle_stride() const { return x_stride_; }
  size_t inner_stride() const { return c_stride_; }
  int outer_limit() const { return height_; }
  int middle_limit() const { return width_; }
  int inner_limit() const { return num_channels_; }
  int row_padding() const {
    return static_cast<int>(y_stride_ - x_stride_ * width_);
  }

  int width_;
  int height_;
  int num_channels_;
  size_t x_stride_;
  size_t y_stride_;
  size_t c_stride_;
  size_t num_samples_;
};

template <>
class TImageStrides<kChannelContiguous> {
 public:
  TImageStrides(int width, int height, int num_channels, int row_padding)
      : width_(width),
        height_(height),
        num_channels_(num_channels),
        x_stride_(1),
        y_stride_(width + row_padding),
        c_stride_(y_stride_ * height),
        num_samples_(c_stride_ * num_channels) {}

  TImageStrides(const TImageStrides &other) = default;
  TImageStrides& operator=(const TImageStrides& other) = default;

  size_t outer_stride() const { return c_stride_; }
  size_t middle_stride() const { return y_stride_; }
  size_t inner_stride() const { return x_stride_; }
  int outer_limit() const { return num_channels_; }
  int middle_limit() const { return height_; }
  int inner_limit() const { return width_; }
  int row_padding() const { return static_cast<int>(y_stride_ - width_); }

  int width_;
  int height_;
  int num_channels_;
  size_t x_stride_;
  size_t y_stride_;
  size_t c_stride_;
  size_t num_samples_;
};

//-----------------------------------------------------------------------------
// Implementation of template class TImageIteratorLoopCounters<layout>, with
// specializations for kPixelContiguous and kChannelContiguous layouts.

template <TImageLayout layout>
struct TImageIteratorLoopCounters {
  explicit TImageIteratorLoopCounters(int c);
  int x() const;
  int y() const;
  int c() const;

  int outer;
  int middle;
  int inner;
  const int c_offset;
};

template <>
struct TImageIteratorLoopCounters<kPixelContiguous> {
  explicit TImageIteratorLoopCounters(int c)
      : outer(0), middle(0), inner(0), c_offset(c) {}

  int x() const { return middle; }
  int y() const { return outer; }
  int c() const { return inner + c_offset; }

  int outer;
  int middle;
  int inner;
  const int c_offset;
};

template <>
struct TImageIteratorLoopCounters<kChannelContiguous> {
  explicit TImageIteratorLoopCounters(int c)
      : outer(0), middle(0), inner(0), c_offset(c) {}

  int x() const { return inner; }
  int y() const { return middle; }
  int c() const { return outer + c_offset; }

  int outer;
  int middle;
  int inner;
  const int c_offset;
};

//-----------------------------------------------------------------------------
// Implementation of template class TImageSampleIterator<T, layout>

template <typename T, TImageLayout layout> inline
T* TImageSampleIterator<T, layout>::pointer() const {
  return inner_pointer_;
}

template <typename T, TImageLayout layout> inline
void TImageSampleIterator<T, layout>::NextSample() {
  // The iterator maintains two sets of loop counters, one based on integer
  // coordinates, and one based on pointers.  Iterating over the samples
  // by stepping pointers is faster than using integer coordinates, but
  // sometimes the loop body needs access to a sample's (x, y, c) coordinates,
  // and computing them from the pointers is slower than keeping track of the
  // coordinates as we update our pointers.  For loops that don't need access
  // to x, y, or c, we rely on the compiler to recognize that the coordinates
  // are never used and to eliminate the code that updates them.
  loop_counters_.inner += 1;
  inner_pointer_ += inner_stride_;
  if (inner_pointer_ >= inner_end_) {
    loop_counters_.middle += 1;
    loop_counters_.inner = 0;
    middle_pointer_ += middle_stride_;
    inner_pointer_ = middle_pointer_;
    inner_end_ = inner_pointer_ + inner_length_;
    if (middle_pointer_ >= middle_end_) {
      loop_counters_.outer += 1;
      loop_counters_.middle = 0;
      outer_pointer_ += outer_stride_;
      inner_pointer_ = middle_pointer_ = outer_pointer_;
      middle_end_ = middle_pointer_ + middle_length_;
      inner_end_ = inner_pointer_ + inner_length_;
    }
  }
}

template <typename T, TImageLayout layout> inline
bool TImageSampleIterator<T, layout>::AtEnd() const {
  return outer_pointer_ >= outer_end_;
}

template <typename T, TImageLayout layout> inline
TImageSampleIterator<T, layout>::TImageSampleIterator(
    const TImageStrides<layout>& strides,
    T* base_pointer,
    int c)
    : loop_counters_(c),
      outer_stride_(strides.outer_stride()),
      middle_stride_(strides.middle_stride()),
      inner_stride_(strides.inner_stride()),
      outer_pointer_(base_pointer),
      middle_pointer_(outer_pointer_),
      inner_pointer_(middle_pointer_),
      middle_length_(strides.middle_limit() * middle_stride_),
      inner_length_(strides.inner_limit() * inner_stride_),
      outer_end_(outer_pointer_ + strides.outer_limit() * outer_stride_),
      middle_end_(middle_pointer_ + middle_length_),
      inner_end_(inner_pointer_ + inner_length_) {
  if (middle_length_ == 0 || inner_length_ == 0) {
    // Make sure that the iterator does not run past the end of an image
    // whose size along one of the dimensions (x, y or c) is zero.
    outer_end_ = outer_pointer_;
  }
}

//-----------------------------------------------------------------------------
// Implementation of template class ReadOnlyTImageView<T, layout>

template <typename T, TImageLayout layout>
ReadOnlyTImageView<T, layout>::ReadOnlyTImageView(
    const ReadOnlyTImageView& other)
    : strides_(other.strides_),
      base_pointer_(other.base_pointer_) {}

template <typename T, TImageLayout layout>
ReadOnlyTImageView<T, layout>::ReadOnlyTImageView(
    const ReadOnlyTImageView& other, int x0, int y0, int x1, int y1)
    : ReadOnlyTImageView(other) {
  FastCrop(x0, y0, x1, y1);
}

template <typename T, TImageLayout layout>
ReadOnlyTImageView<T, layout>::ReadOnlyTImageView(
    const ReadOnlyTImageView& other, int x0, int y0, int c0, int x1, int y1,
    int c1)
    : ReadOnlyTImageView(other) {
  FastCrop(x0, y0, c0, x1, y1, c1);
}

template <typename T, TImageLayout layout>
ReadOnlyTImageView<T, layout>::ReadOnlyTImageView(
    const TImageStrides<layout>& strides,
    SampleType* base_pointer) : strides_(strides),
                                base_pointer_(base_pointer) {}

template <typename T, TImageLayout layout>
ReadOnlyTImageView<T, layout>::ReadOnlyTImageView(
    int width, int height, int num_channels, SampleType* base_pointer,
    size_t row_padding)
    : ReadOnlyTImageView(
          TImageStrides<layout>(width, height, num_channels, row_padding),
          base_pointer) {
}

template <typename T, TImageLayout layout>
ReadOnlyTImageView<T, layout>& ReadOnlyTImageView<T, layout>::
    operator=(const ReadOnlyTImageView<T, layout>& other) {
  strides_ = other.strides_;
  base_pointer_ = other.base_pointer_;
  return *this;
}

template <typename T, TImageLayout layout>
ReadOnlyTImageView<T, layout>::~ReadOnlyTImageView() {}

template <typename T, TImageLayout layout> inline
const T& ReadOnlyTImageView<T, layout>::at(int x, int y, int c) const {
  return base_pointer_[x * x_stride() + y * y_stride() + c * c_stride()];
}

template <typename T, TImageLayout layout> inline
typename ReadOnlyTImageView<T, layout>::ConstSampleIterator
ReadOnlyTImageView<T, layout>::sample_iterator() const {
  return ConstSampleIterator(strides_, base_pointer_, 0);
}

template <typename T, TImageLayout layout> inline
typename ReadOnlyTImageView<T, layout>::ConstSampleIterator
ReadOnlyTImageView<T, layout>::sample_iterator(int c) const {
  auto tmp_strides = strides_;
  tmp_strides.num_channels_ = 1;
  const T* tmp_base_pointer = base_pointer_ + c * strides_.c_stride_;
  return ConstSampleIterator(tmp_strides, tmp_base_pointer, c);
}

template <typename T, TImageLayout layout>
void ReadOnlyTImageView<T, layout>::FastCrop(int x0, int y0, int x1, int y1) {
  FastCrop(x0, y0, 0, x1, y1, num_channels());
}

template <typename T, TImageLayout layout>
void ReadOnlyTImageView<T, layout>::FastCrop(int x0, int y0, int c0, int x1,
                                             int y1, int c1) {
  if (*this == nullptr) {
    return;
  }

  x0 = std::max(x0, 0);
  y0 = std::max(y0, 0);
  c0 = std::max(c0, 0);
  x1 = std::min(x1, strides_.width_);
  y1 = std::min(y1, strides_.height_);
  c1 = std::min(c1, strides_.num_channels_);
  base_pointer_ += x0 * strides_.x_stride_ + y0 * strides_.y_stride_ +
                   c0 * strides_.c_stride_;
  strides_.width_  = std::max(0, x1 - x0);
  strides_.height_ = std::max(0, y1 - y0);
  strides_.num_channels_ = std::max(0, c1 - c0);
}

template <typename T, TImageLayout layout>
bool ReadOnlyTImageView<T, layout>::SamplesAreCompact() const {
  // Compute the distance, measured in samples, between the first sample and
  // the location right after the last sample in this image view.  If there
  // is no padding between rows or channels, then this distance is equal to
  // the number of accessible samples (width * height * number of channels).
  size_t num_samples =
      &at(width() - 1, height() - 1, num_channels() - 1) - &at(0, 0, 0) + 1;
  return num_samples == width() * height() * num_channels();
}

template <typename T, TImageLayout layout>
inline size_t ReadOnlyTImageView<T, layout>::sample_array_size() const {
  return strides_.num_samples_ * sizeof(T);
}

//-----------------------------------------------------------------------------
// Implementation of template class ReadWriteTImageView<T, layout>

template <typename T, TImageLayout layout>
ReadWriteTImageView<T, layout>::ReadWriteTImageView(
    const ReadWriteTImageView& other)
    : ReadOnlyView(other) {}

template <typename T, TImageLayout layout>
ReadWriteTImageView<T, layout>::ReadWriteTImageView(
    const ReadWriteTImageView& other, int x0, int y0, int x1, int y1)
    : ReadOnlyView(other, x0, y0, x1, y1) {}

template <typename T, TImageLayout layout>
ReadWriteTImageView<T, layout>::ReadWriteTImageView(
    const ReadWriteTImageView& other, int x0, int y0, int c0, int x1, int y1,
    int c1)
    : ReadOnlyView(other, x0, y0, c0, x1, y1, c1) {}

template <typename T, TImageLayout layout>
ReadWriteTImageView<T, layout>::ReadWriteTImageView(
    const TImageStrides<layout>& strides, SampleType* base_pointer)
    : ReadOnlyView(strides, base_pointer) {}

template <typename T, TImageLayout layout>
ReadWriteTImageView<T, layout>::ReadWriteTImageView(
    int width, int height, int num_channels, SampleType* base_pointer,
    size_t row_padding)
    : ReadWriteTImageView(
          TImageStrides<layout>(width, height, num_channels, row_padding),
          base_pointer) {
}

template <typename T, TImageLayout layout>
ReadWriteTImageView<T, layout>::~ReadWriteTImageView() {}

template <typename T, TImageLayout layout>
ReadWriteTImageView<T, layout>& ReadWriteTImageView<T, layout>::
    operator=(const ReadWriteTImageView<T, layout>& other) {
  strides_ = other.strides_;
  base_pointer_ = other.base_pointer_;
  return *this;
}

template <typename T, TImageLayout layout> inline
T& ReadWriteTImageView<T, layout>::at(int x, int y, int c) const {
  return base_pointer_[x * x_stride() + y * y_stride() + c * c_stride()];
}

template <typename T, TImageLayout layout> inline
typename ReadWriteTImageView<T, layout>::SampleIterator
ReadWriteTImageView<T, layout>::sample_iterator() const {
  return SampleIterator(strides_, base_pointer_, 0);
}

template <typename T, TImageLayout layout> inline
typename ReadWriteTImageView<T, layout>::SampleIterator
ReadWriteTImageView<T, layout>::sample_iterator(int c) const {
  auto tmp_strides = strides_;
  tmp_strides.num_channels_ = 1;
  T* tmp_base_pointer = base_pointer_ + c * ReadOnlyView::strides_.c_stride_;
  return SampleIterator(tmp_strides, tmp_base_pointer, c);
}

template <typename T, TImageLayout layout>
void ReadWriteTImageView<T, layout>::Fill(
    const SampleType v, int x0, int y0, int x1, int y1) const {
  ReadWriteTImageView target(*this, x0, y0, x1, y1);

  for (auto i = target.sample_iterator(); !i.AtEnd(); i.NextSample()) {
    *i = v;
  }
}

template <typename T, TImageLayout layout>
void ReadWriteTImageView<T, layout>::Fill(const SampleType v) const {
  if (SamplesAreCompact()) {
    // Optimized fill for the case where the image view is not padded.
    SampleType* begin = base_pointer();
    SampleType* end = begin + width() * height() * num_channels();
    for (SampleType* sample = begin; sample < end; ++sample) {
      *sample = v;
    }
  } else {
    // Regular fill.
    Fill(v, 0, 0, width(), height());
  }
}

template <typename T, TImageLayout layout>
template <typename SourceType>
void ReadWriteTImageView<T, layout>::CopyFrom(
  const SourceType& source,
  int x0, int y0, int x1, int y1,
  int x, int y) const {
  // Crop the source and destination regions if they extend beyond
  // the edges of the source and destination images.
  if (x0 < 0) {
    x -= x0;
    x0 = 0;
  }
  if (y0 < 0) {
    y -= y0;
    y0 = 0;
  }

  if (x1 > source.width()) {
    x1 = source.width();
  }
  if (y1 > source.height()) {
    y1 = source.height();
  }

  if (x < 0) {
    x0 -= x;
    x = 0;
  }
  if (y < 0) {
    y0 -= y;
    y = 0;
  }

  // Copy the pixels, converting the values from the source region's
  // SampleType to this region's SampleType if necessary.
  ReadWriteTImageView target(*this, x, y, x + (x1 - x0), y + (y1 - y0));

  if (source.num_channels() < num_channels()) {
    for (int c = 0; c < source.num_channels(); ++c) {
      for (auto i = target.sample_iterator(c); !i.AtEnd(); i.NextSample()) {
        *i = static_cast<SampleType>(source.at(i.x() + x0, i.y() + y0, c));
      }
    }
  } else {
    for (auto i = target.sample_iterator(); !i.AtEnd(); i.NextSample()) {
      *i = static_cast<SampleType>(source.at(i.x() + x0, i.y() + y0, i.c()));
    }
  }
}

template <typename T1, typename T2>
struct CompareSampleTypes { enum { Equal = false }; };
template <typename T1>
struct CompareSampleTypes<T1, T1> { enum { Equal = true }; };

template <typename T, TImageLayout layout>
template <typename SourceType>
void ReadWriteTImageView<T, layout>::CopyFrom(const SourceType& source) const {
  if (CompareSampleTypes<typename SourceType::SampleType, SampleType>::Equal &&
      (source.sample_layout() == sample_layout() ||
          (source.num_channels() == 1 && num_channels() == 1)) &&
      source.width() == width() &&
      source.height() == height() &&
      source.num_channels() == num_channels() &&
      source.SamplesAreCompact() && SamplesAreCompact()) {
    // Optimized copy for the case where the source and destination image
    // views have the same sample types, layout, width, height and number
    // of channels, and neither view is padded. (Note that for single-channel
    // image views the kPixelContigous and kChannelContiguous layouts are
    // the same.)
    memcpy(base_pointer(), source.base_pointer(),
           width() * height() * num_channels() * sizeof(SampleType));
  } else {
    // Regular copy.
    CopyFrom(source, 0, 0, width(), height(), 0, 0);
  }
}

//-----------------------------------------------------------------------------
// Implementation of template class TImage<T, layout>

template <typename T, TImageLayout layout>
TImage<T, layout>::TImage(int width, int height, int num_channels,
                          TImageInit init, size_t row_padding,
                          TImageSampleAllocator* allocator)
    : ReadWriteView(
          TImageStrides<layout>(width, height, num_channels, row_padding),
          /*base_pointer=*/nullptr) {
  allocator_ = allocator;
  assert(allocator_ != nullptr);
  assert(strides_.width_ >= 0);
  assert(strides_.height_ >= 0);
  assert(strides_.num_channels_ >= 1);

  memory_ = AllocateMemory(strides_.num_samples_);
  base_pointer_ = memory_;

  if (init == kInitZero) {
    memset(base_pointer_, 0, strides_.num_samples_ * sizeof(SampleType));
  }
}

template <typename T, TImageLayout layout>
TImage<T, layout>::TImage(int width, int height, int num_channels,
                          size_t row_padding, SampleType* base_pointer,
                          TImageSampleAllocator* allocator)
    : ReadWriteView(
          TImageStrides<layout>(width, height, num_channels, row_padding),
          base_pointer) {
  allocator_ = allocator;
  assert(allocator_ != nullptr);
  assert(strides_.width_ >= 0);
  assert(strides_.height_ >= 0);
  assert(strides_.num_channels_ >= 1);
  memory_ = base_pointer_;
}

template <typename T, TImageLayout layout>
TImage<T, layout>::TImage(const TImage& other)
    : ReadWriteView(other.strides_, /*base_pointer=*/nullptr),
      allocator_(other.allocator_) {
  assert(allocator_ != nullptr);
  if (other) {
    memory_ = AllocateMemory(strides_.num_samples_);
    memcpy(memory_, other.memory_, strides_.num_samples_ * sizeof(SampleType));
    base_pointer_ = memory_ + (other.base_pointer_ - other.memory_);
  } else {
    memory_ = nullptr;
    base_pointer_ = nullptr;
  }
}

template <typename T, TImageLayout layout>
TImage<T, layout>::TImage(TImage&& other) : TImage() {
  strides_ = other.strides_;
  base_pointer_ = other.base_pointer_;
  memory_ = other.memory_;
  allocator_ = other.allocator_;

  other.strides_ = TImageStrides<layout>(0, 0, 0, 0);
  other.base_pointer_ = nullptr;
  other.memory_ = nullptr;
  other.allocator_ = TImageDefaultSampleAllocator();
}

template <typename T, TImageLayout layout>
TImage<T, layout>& TImage<T, layout>::operator=(TImage&& other) {
  if (this != &other) {
    ReleaseMemory();

    strides_ = other.strides_;
    base_pointer_ = other.base_pointer_;
    memory_ = other.memory_;
    allocator_ = other.allocator_;

    other.strides_ = TImageStrides<layout>(0, 0, 0, 0);
    other.base_pointer_ = nullptr;
    other.memory_ = nullptr;
    other.allocator_ = TImageDefaultSampleAllocator();
  }
  return *this;
}

template <typename T, TImageLayout layout>
TImage<T, layout>& TImage<T, layout>::operator=(const TImage& other) {
  if (this != &other) {
    ReleaseMemory();

    strides_ = other.strides_;
    allocator_ = other.allocator_;
    assert(allocator_ != nullptr);
    if (other) {
      memory_ = AllocateMemory(strides_.num_samples_);
      memcpy(memory_, other.memory_,
             strides_.num_samples_ * sizeof(SampleType));
      base_pointer_ = memory_ + (other.base_pointer_ - other.memory_);
    } else {
      memory_ = nullptr;
      base_pointer_ = nullptr;
    }
  }
  return *this;
}

template <typename T, TImageLayout layout>
TImage<T, layout>::~TImage() {
  ReleaseMemory();
}

template <typename T, TImageLayout layout>
TImage<T, layout>& TImage<T, layout>::operator=(std::nullptr_t) {
  ReleaseMemory();
  return *this;
}

template <typename T, TImageLayout layout>
T* TImage<T, layout>::AllocateMemory(size_t num_samples) const {
  return reinterpret_cast<SampleType*>(
      allocator_->Allocate(num_samples * sizeof(SampleType)));
}

template <typename T, TImageLayout layout>
void TImage<T, layout>::ReleaseMemory() {
  if (*this) {
    allocator_->Deallocate(memory_, strides_.num_samples_ * sizeof(SampleType));
  }
  memory_ = nullptr;
  base_pointer_ = nullptr;
  allocator_ = TImageDefaultSampleAllocator();
  strides_ = TImageStrides<layout>(0, 0, 0, 0);
}

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_T_IMAGE_H_
