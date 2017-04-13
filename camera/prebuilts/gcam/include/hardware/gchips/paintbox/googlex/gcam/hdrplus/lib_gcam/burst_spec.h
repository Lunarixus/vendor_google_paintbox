#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_BURST_SPEC_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_BURST_SPEC_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <string>
#include <vector>

#include "googlex/gcam/ae/ae_shot_params.h"  // Just for HdrMode.
#include "googlex/gcam/base/log_level.h"
#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/frame_request.h"
#include "hardware/gchips/paintbox/googlex/gcam/hdrplus/lib_gcam/gcam_types.h"
#include "googlex/gcam/image_metadata/awb_info.h"

namespace gcam {

enum BurstType {
  kMetering = 0,  // XXXXXX           (all kSingleExp)
  kHomogenous,    // XXXX/XXXXXXXXXX  (all kSingleExp)
  kPoorMans,      // SSSSSSSSSS       (all kShortExp)
  kInvalidBurstType
  // where L = kLongExp, S = kShortExp, X = kSingleExp (motion fallback).
};

// TODO(geiss): Remove this sub-struct? The first two fields can be recovered
// from AeResults and the rest are just intermediate results for computing
// BurstSpec::FrameRequest::awb. Doing this would require saving AeResults when
// saving brusts (which is a good idea anyway).
struct PoorMansParams {
  PoorMansParams() { Clear(); }
  void Clear();

  float   long_tet_over_short_tet;
  float   fraction_of_pixels_from_long_exposure;
  AwbInfo short_ideal_awb;
  AwbInfo long_ideal_awb;
};

struct BurstSpec {
  BurstSpec() { Clear(); }
  void Clear();
  void Print(LogLevel log_level) const;
  void SerializeToString(std::string* str) const;
  bool DeserializeFromString(const char** str);

  // This vector describes each frame that Gcam wants the client to capture.
  std::vector<FrameRequest> frame_requests;

  // The remaining information is internal to Gcam, and should not be modified
  // by the client.
  BurstType                 type;
  // TODO(geiss): Remove 'scene_is_hdr', as it's been storing the wrong value
  // (it has actually been storing whether or not we can run HDR on the scene).
  int                       scene_is_hdr;    // Usually 0 or 1; -1 = unknown.
  HdrMode                   hdr_mode;
  float                     exp_comp;
  PoorMansParams            poor_mans;  // Only valid if (type == kPoorMans).
};

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_BURST_SPEC_H_
