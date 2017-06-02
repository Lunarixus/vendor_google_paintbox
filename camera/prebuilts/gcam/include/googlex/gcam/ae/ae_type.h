#ifndef GOOGLEX_GCAM_AE_AE_TYPE_H_
#define GOOGLEX_GCAM_AE_AE_TYPE_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

namespace gcam {

enum AeType {
  kHdrShort = 0,    // The ideal exposure to ensure capturing most highlights.
  kHdrLong,         // The ideal exposure for the midtones and shadows.
  kAeTypeCount
};

const char* ToText(AeType type);
AeType TextToAeType(const char* text);

}  // namespace gcam

#endif  // GOOGLEX_GCAM_AE_AE_TYPE_H_
