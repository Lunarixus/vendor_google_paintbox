#ifndef GOOGLEX_GCAM_BASE_PIXEL_RECT_H_
#define GOOGLEX_GCAM_BASE_PIXEL_RECT_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <algorithm>
#include <string>

#include "googlex/gcam/base/log_level.h"

namespace gcam {

// TODO(geiss): Use PixelRect and NormalizedRect throughout Gcam.
struct PixelRect {
  // [0, 0] = Upper-left corner of image.
  // The exact area of pixels actually covered is:
  //   [x0 .. x1 - 1] by [y0 .. y1 - 1]
  // A rectangle where { x0, x1, y0, y1 } are set to { 0, w, 0, h }
  //   will exactly cover the full image.
  int x0;     // Range: [0 .. x1).
  int x1;     // Range: (x0 .. width].
  int y0;     // Range: [0 .. y1).
  int y1;     // Range: (y0 .. height].

  bool IsEmpty() const;
  bool Check() const;
  void Print(LogLevel log_level, int indent_spaces) const;
  void SerializeToString(std::string* str, int indent_spaces) const;
  bool DeserializeFromString(const char** str_ptr);
  bool Equals(const PixelRect& other) const;

  float AspectRatio() const;
  float InverseAspectRatio() const;
  int width() const { return x1 - x0; }
  int height() const { return y1 - y0; }
};

// Return the intersection of two PixelRects.
inline PixelRect Intersect(const PixelRect& a, const PixelRect& b) {
  return PixelRect {
    std::max(a.x0, b.x0),
    std::min(a.x1, b.x1),
    std::max(a.y0, b.y0),
    std::min(a.y1, b.y1)
  };
}

struct NormalizedRect {
  // [0,0] = Upper-left corner of image.
  // [1,1] = Lower-right corner of image.
  float x0 = 0;     // Range: [0 .. x1).
  float x1 = 1;     // Range: (x0 .. 1].
  float y0 = 0;     // Range: [0 .. y1).
  float y1 = 1;     // Range: (y0 .. 1].

  bool Check() const;
  void Print(LogLevel log_level, int indent_spaces) const;
  void SerializeToString(std::string* str, int indent_spaces) const;
  bool DeserializeFromString(const char** str_ptr);
  bool Equals(const NormalizedRect& other) const;

  // (width / height).
  float AspectRatio() const;

  // (height / width).
  float InverseAspectRatio() const;

  float width() const { return x1 - x0; }
  float height() const { return y1 - y0; }
};

// Represents a rectangle (within [0,0]...[1,1]) and a weight value associated
// with it.  There are no constraints on the weight value here, although some
// particular use cases might place constraints on it (e.g. ShotParam's
// weighted metering areas, which require the weight be > 0).
struct WeightedRect {
  NormalizedRect rect;
  float          weight;

  void Print(LogLevel log_level, int indent_spaces) const;
  void SerializeToString(std::string* str, int indent_spaces) const;
  bool DeserializeFromString(const char** str_ptr);
  bool Equals(const WeightedRect& other) const;
};

}  // namespace gcam

#endif  // GOOGLEX_GCAM_BASE_PIXEL_RECT_H_
