#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_IMAGE_METADATA_CLIENT_EXIF_METADATA_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_IMAGE_METADATA_CLIENT_EXIF_METADATA_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <cstdint>
#include <string>

#include "hardware/gchips/paintbox/googlex/gcam/image/t_image.h"

namespace gcam {

struct LocationData {
  LocationData() { Clear(); }
  void Clear();
  bool Check() const;

  double latitude;                // Latitude, in degrees
  double longitude;               // Longitude, in degrees
  double altitude;                // Altitude, in meters
  double degree_of_precision;     // Degree of precision, HDOP (2D) or PDOP (3D)
  int64_t timestamp_unix;         // UTC time, in seconds since January 1, 1970
  std::string processing_method;  // Name of the method for location finding
};

// Optional, client-provided subset of the EXIF metadata to be encoded when
// writing an image to a JPG or DNG file. In normal usage of the Gcam pipeline,
// the rest of the EXIF metadata is populated automatically (from static
// metadata, shot parameters, and frame metadata).
struct ClientExifMetadata {
  ClientExifMetadata() { Clear(); }
  void Clear() { thumbnail = nullptr; }

  // Location where the shot was taken.
  LocationData location;

  // Client-provided RGB image for use as JPEG thumbnail (optional). This field
  // is useful if a hardware-generated thumbnail is available. More typically,
  // this field is left as null, in which case a thumbnail image will be
  // generated automatically. Note that when 'thumbnail' is provided, its
  // orientation must match the main image.
  //
  // Client keeps ownership of the thumbnail, and needs to hold off on deleting
  // it until the final JPG is returned via the callback.
  InterleavedImageU8* thumbnail;
};

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_IMAGE_METADATA_CLIENT_EXIF_METADATA_H_
