#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_DEBUG_PARAMS_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_DEBUG_PARAMS_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <cstdint>

namespace gcam {

// Flags for saving intermediate image results, used to construct
// DebugParams::save_bitmask.
static const uint32_t GCAM_SAVE_NONE                    = 0u;
static const uint32_t GCAM_SAVE_INPUT_METERING          = (1u << 18);
static const uint32_t GCAM_SAVE_INPUT_PAYLOAD           = (1u << 19);
static const uint32_t GCAM_SAVE_TEXT                    = (1u << 29);
// Add IPU watermark to finished images, only if "use_ipu" flag is set for gcam;
// otherwise this flag has no effect
static const uint32_t GCAM_SAVE_IPU_WATERMARK           = (1u << 30);

// Debugging options, including what debugging information gets saved to disk.
// These parameters are configured at Gcam startup, and are normally fixed.
// However, they can be overridden, per-shot, for tools that reprocess bursts
// from various devices, or with varying settings.
struct DebugParams {
  void Print() const;

  // Bitmask describing what debugging information and intermediate images
  // to save to disk, generated by or'ing GCAM_SAVE_* flags together.
  uint32_t save_bitmask = 0;
};

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_DEBUG_PARAMS_H_
