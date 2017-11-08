#ifndef GOOGLEX_GCAM_IMAGE_METADATA_FLASH_H_
#define GOOGLEX_GCAM_IMAGE_METADATA_FLASH_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <string>

namespace gcam {

// Describes the setting for the flash mode from a UI point of view.
enum class FlashMode {
  kAuto = 0,  // Flash fires automatically.
  kOn,        // Flash forced on.
  kOff,       // Flash forced off.
  kInvalid    // Invalid flash mode.
};

std::string ToText(FlashMode mode);
FlashMode TextToFlashMode(const std::string& text);

// Describes the state of the flash during the capture of a frame.
enum class FlashMetadata {
  kOff,     // Flash was off during the frame capture.
  kOn,      // Flash was on during the frame capture (strobe or torch mode).
  kUnknown  // Flash state not known.
};

std::string ToText(FlashMetadata flash);
FlashMetadata TextToFlashMetadata(const std::string& text);

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_METADATA_FLASH_H_
