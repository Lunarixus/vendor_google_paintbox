#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_TET_MODEL_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_TET_MODEL_H_

#include <string>

#include "hardware/gchips/paintbox/googlex/gcam/base/log_level.h"
#include "hardware/gchips/paintbox/googlex/gcam/image_metadata/frame_metadata.h"

namespace gcam {

class LogSaver;

// TET Factorization Model

// Total exposure time (TET) is the product of the exposure time, in ms, and
//   the overall gain (the product of analog gain and digital gain).
// Note that because the TET includes both analog and digital gain, the range
//   for the overall gain is usually [1..32x] (typically 8x analog, 4x
//   digital).
// The TET values are the output of the AE library.  However, in order to
//   capture a burst, you must factor the TET into an exposure time, analog
//   gain for capture, and a digital gain (to apply either during capture or
//   later).  This model describes how to break the TETs down into these
//   3 factors.
// Note that the waypoints might ideally change, depending on focal length
//   or FOV of the camera (wider-FOV cameras are less sensitive to hand
//   shake, and can tolerate longer exposure times; narrower FOV cameras,
//   or zoomed-in cameras, can tolerate less, and should crank the gain
//   up earlier).
//
struct TetWaypoint {
  float exposure_time_ms;  // Milliseconds.
  float overall_gain;      // Analog gain * digital gain.
                           // 1 = no gain, 4 = 4x gain, etc.
                           // Usually [1..32].
  void Print(LogLevel log_level, int indent_spaces) const;
  void SerializeToString(std::string* str, int indent_spaces) const;
  bool DeserializeFromString(const char** str);
  bool Equals(const TetWaypoint& other) const;
};

inline TetWaypoint MakeTetWaypoint(float exposure_time_ms, float overall_gain) {
  TetWaypoint ret;
  ret.exposure_time_ms = exposure_time_ms;
  ret.overall_gain = overall_gain;
  return ret;
}

static const int kMaxTetWaypoints = 16;

// Note that a TET model should cover everything from exposure_time of 0 and
// gain of 1, to an exposure time of 9999 ms and a gain of 16 (or whatever the
// max gcam supports is).
struct TetModel {
  TetModel() { Clear(); }
  void Clear();
  bool Check() const;
  void Print(LogLevel log_level, int indent_spaces) const;
  void SerializeToString(std::string* str, int indent_spaces) const;
  bool DeserializeFromString(const char** str);
  bool Equals(const TetModel& other) const;

  TetWaypoint model[kMaxTetWaypoints];
  int count;
};

// Factor a TET into exposure time and gain, given a TET factorization model,
// constraints on the range of exposure time and gain, and whether to apply
// antibanding if scene flicker was detected.
void FactorizeTet(const TetModel& model,
                  const float min_exposure_time_ms,
                  const float max_exposure_time_ms,
                  const float max_analog_gain,
                  const float max_digital_gain,
                  const float digital_gain_coming_later_from_phdr,
                  const SceneFlicker scene_flicker,
                  const bool apply_antibanding,
                  const float tet,
                  float* exposure_time_ms_out,
                  float* analog_gain_out,
                  float* digital_gain_out,
                  LogSaver* log_saver);

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_TET_MODEL_H_
