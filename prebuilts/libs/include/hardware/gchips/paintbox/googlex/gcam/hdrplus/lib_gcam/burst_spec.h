#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_BURST_SPEC_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_BURST_SPEC_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <string>
#include <vector>

#include "googlex/gcam/ae/ae_shot_params.h"  // Just for HdrMode.
#include "googlex/gcam/base/log_level.h"
#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/frame_request.h"
#include "googlex/gcam/image_metadata/awb_info.h"

namespace gcam {

struct BurstSpec {
  BurstSpec() { Clear(); }
  void Clear();
  void Print(LogLevel log_level) const;
  void SerializeToString(std::string* str) const;
  bool DeserializeFromString(const char** str);

  // This vector describes each frame that Gcam wants the client to capture.
  std::vector<FrameRequest> frame_requests;
};

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_BURST_SPEC_H_
