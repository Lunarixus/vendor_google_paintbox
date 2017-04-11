#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_GCAM_TYPES_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_GCAM_TYPES_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

// Assorted publicly-visible lightweight types used by the Gcam pipeline, and
// some associated utilities.

namespace gcam {

// Describes how Gcam will perform metering for a shot.  For more details on
// smart metering, see gcam.h.
enum class MeteringMode {
  kBurst = 0,    // An explicit metering burst.
  kLazySmart,    // Lazy smart metering.
  kEagerSmart,   // Eager smart metering.
};

// Specifies a custom [platform-specific] thread priority for a new thread.
// If 'explicitly_set' is false, then the thread that executes the task
//   will inherit the priority of the caller.
// If 'explicitly_set' is true, then the thread's priority will be set to
//   'value', the meaning of which depends on platform:
//     Linux      Values are Nice values.
//     Android    Values are Android thread priorities: (~Nice values)
//                    19   lowest
//                    10   background
//                     0   normal
//                    -2   foreground
//                    -4   screen
//                   -20   highest
//   For a full list of Android priority values, see:
//     https://cs.corp.google.com/#android/system/core/include/system/thread_defs.h  //NOLINT
//
struct ThreadPriority {
  bool Equals(const ThreadPriority& other) const;

  bool explicitly_set;
  int  value;
};

enum GcamInputFrameType {
  kUnknownFrameType = 0,
  kMeteringFrame,
  kPayloadFrame,
  kPayloadAuxFrame,  // Deprecated.
  kViewfinderFrame,
};

enum class Stage : int {
  kNone = 0,
  kAlign,
  kMerge,
  kDemosaic,
  kChromaDenoise,
  kLocalTonemap,  // HDR shots only.
  kDenoise,
  kRetonemap,
  kDehaze,
  kSharpen,
  kFinal,
  kStageCount,
};

// These enums only apply to the finish pipe
enum ExecuteOn: int {
  kExecuteOnCpu = 0,
  kExecuteOnCpuWithIpuAlgorithm,
  kExecuteOnHexagon,
  kExecuteOnIpu,
  kExecuteOnIpuStriped,  // IPU execution with striping.
};


}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_GCAM_TYPES_H_
