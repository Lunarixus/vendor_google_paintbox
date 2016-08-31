#ifndef GOOGLEX_GCAM_IMAGE_RAW_BUFFER_LAYOUT_H_
#define GOOGLEX_GCAM_IMAGE_RAW_BUFFER_LAYOUT_H_

#include "third_party/halide/halide/src/runtime/HalideRuntime.h"

namespace gcam {

// Describes the buffer layout of Bayer raw data.
enum class RawBufferLayout : int {
  // 10-bit packed data stored in a 1-channel uint8 buffer.
  // 4 10-bit pixels a, b, c, d are laid out in 5 bytes of memory like so:
  //
  //   0: [a9 a8 a7 a6 a5 a4 a3 a2]
  //   1: [b9 b8 b7 b6 b5 b4 b3 b2]
  //   2: [c9 c8 c7 c6 c5 c4 c3 c2]
  //   3: [d9 d8 d7 d6 d5 d4 d3 d2]
  //   4: [d1 d0 c1 c0 b1 b0 a1 a0]
  //
  // This is equivalent to Android's RAW10 format:
  // http://developer.android.com/reference/android/graphics/ImageFormat.html#RAW10
  // //NOLINT
  kRaw10,

  // 12-bit data stored in a 1-channel uint8 buffer.
  // 2 12-bit pixels a and b are laid out in 3 bytes of memory like so:
  //
  //    0: [a11 a10 a9  a8  a7  a6  a5  a4 ]
  //    1: [b11 b10 b9  b8  b7  b6  b5  b4 ]
  //    2: [b3  b2  b1  b0  a3  a2  a1  a0 ]
  //
  // This is equivalent to Android's RAW12 format:
  // http://developer.android.com/reference/android/graphics/ImageFormat.html#RAW12
  // //NOLINT
  kRaw12,

  // 16-bit data stored in an interleaved 1-channel uint16 buffer. Bayer
  // channels c0, c1, c2, c3 are laid out in rows of memory like so:
  //
  //   0: c0 c1 c0 c1 ...
  //   1: c2 c3 c2 c3 ...
  //   2: c0 c1 c0 c1 ...
  //   3: c2 c3 c2 c3 ...
  //   ...
  //
  // This is equivalent to Android's RAW_SENSOR format:
  // http://developer.android.com/reference/android/graphics/ImageFormat.html#RAW_SENSOR
  // //NOLINT
  kRaw16,

  // 16-bit data stored in a deinterleaved 4-channel uint16 buffer. Bayer
  // channels c0, c1, c2, c3 are stored in separate planes contiguously in
  // memory.
  kRawPlanar16,
};

// Check if the buffer layout is planar.
inline bool IsPlanar(RawBufferLayout layout) {
  switch (layout) {
    case RawBufferLayout::kRawPlanar16: return true;
    default: return false;
  }
}

// Get the bounds of a buffer that may have a variety of input layouts.
void GetRawBufferBounds(RawBufferLayout layout, buffer_t raw,
                        int* min_x, int* min_y, int* extent_x, int* extent_y);

// Shift the mins of a raw buffer, accounting for the buffer layout. dx, dy are
// the shifts as specified for a planar image, i.e. dx, dy represent 2 pixel
// steps. One pixel shifts are not representable, as is expected for raw data.
buffer_t AdjustRawBufferMins(RawBufferLayout layout, buffer_t raw,
                             int dx, int dy);

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_RAW_BUFFER_LAYOUT_H_
