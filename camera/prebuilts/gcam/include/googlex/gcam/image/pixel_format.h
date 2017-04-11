#ifndef GOOGLEX_GCAM_IMAGE_PIXEL_FORMAT_H_
#define GOOGLEX_GCAM_IMAGE_PIXEL_FORMAT_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

namespace gcam {

enum class GcamPixelFormat {
  kUnknown = 0,
  kNv12,  // A full-res Y plane, plus a half-res interleaved UVUV... plane.
  kNv21,  // A full-res Y plane, plus a half-res interleaved VUVU... plane.
  kRgb,
  kBgr,
  kRgba,
  kBgra,
  kArgb,
  kAbgr,
  // ...insert new types here.
  kCount
};

const char* ToText(GcamPixelFormat format);
GcamPixelFormat TextToGcamPixelFormat(const char* text);

inline bool IsYuv(GcamPixelFormat format) {
  return (format == GcamPixelFormat::kNv12 || format == GcamPixelFormat::kNv21);
}

inline bool IsRgb(GcamPixelFormat format) {
  return (format == GcamPixelFormat::kRgb ||
          format == GcamPixelFormat::kBgr ||
          format == GcamPixelFormat::kArgb ||
          format == GcamPixelFormat::kAbgr ||
          format == GcamPixelFormat::kRgba ||
          format == GcamPixelFormat::kBgra);
}

inline int GetBitsPerPixel(GcamPixelFormat format) {
  switch (format) {
    case GcamPixelFormat::kNv12:
    case GcamPixelFormat::kNv21:
      return 12;
    case GcamPixelFormat::kRgb:
    case GcamPixelFormat::kBgr:
      return 24;
    case GcamPixelFormat::kRgba:
    case GcamPixelFormat::kBgra:
    case GcamPixelFormat::kArgb:
    case GcamPixelFormat::kAbgr:
      return 32;
    case GcamPixelFormat::kUnknown:
    default:
      return 0;
  }
}

inline int GetNumChannels(GcamPixelFormat format) {
  switch (format) {
    case GcamPixelFormat::kNv12:
    case GcamPixelFormat::kNv21:
    case GcamPixelFormat::kRgb:
    case GcamPixelFormat::kBgr:
      return 3;
    case GcamPixelFormat::kRgba:
    case GcamPixelFormat::kBgra:
    case GcamPixelFormat::kArgb:
    case GcamPixelFormat::kAbgr:
      return 4;
    case GcamPixelFormat::kUnknown:
    default:
      return 0;
  }
}

GcamPixelFormat GetRandomPixelFormat();

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_PIXEL_FORMAT_H_
