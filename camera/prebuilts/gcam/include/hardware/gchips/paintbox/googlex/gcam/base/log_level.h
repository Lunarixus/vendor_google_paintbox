#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_BASE_LOG_LEVEL_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_BASE_LOG_LEVEL_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

namespace gcam {

// Log severity level, modeled on Android's convention.
// http://developer.android.com/reference/android/util/Log.html
// The constants are chosen for consistency with //base/port/platform_log.h,
// which specifies a mapping between google3 and Android log levels.
enum class LogLevel {
  kLogNever = -100,
  kLogV = -2,        // ANDROID_LOG_VERBOSE   VLOG(2) .. VLOG(N)
  kLogD = -1,        // ANDROID_LOG_DEBUG     VLOG(1)
  kLogI = 0,         // ANDROID_LOG_INFO      LOG(INFO), VLOG(0)
  kLogW = 1,         // ANDROID_LOG_WARN      LOG(WARNING)
  kLogE = 2,         // ANDROID_LOG_ERROR     LOG(ERROR)
  kLogF = 3,         // ANDROID_LOG_FATAL     LOG(FATAL)
};

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_BASE_LOG_LEVEL_H_
