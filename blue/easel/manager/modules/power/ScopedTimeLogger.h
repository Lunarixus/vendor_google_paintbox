#ifndef POWER_SCOPEDTIMELOGGER_H_
#define POWER_SCOPEDTIMELOGGER_H_

#include <chrono>
#include <string>

#include "android-base/logging.h"

/*
 * Use "-DENABLE_SCOPED_TIME_LOGGER" to enable this scoped time logger
 * at compilation time.
 */
#if defined(ENABLE_SCOPED_TIME_LOGGER)
#define MEASURE_SCOPED_TIME(_description) \
  android::EaselPowerBlue::ScopedTimeLogger _logger(_description)
#else
#define MEASURE_SCOPED_TIME(_description) \
  do {} while(0);
#endif  // ENABLE_SCOPED_TIME_LOGGER

namespace android {
namespace EaselPowerBlue {

/*
 * ScopedTimeLogger
 *
 * ScopedTimeLogger class measures wall clock duration between initialization
 * of the class and the end of scope.  The duration will be calculated
 * automatically in destructor, and be printed in milliseconds at INFO level,
 * along with the name initialized with this ScopedTimeLogger.
 *
 */
class ScopedTimeLogger {
 public:
  ScopedTimeLogger(const char *name) : mName(name) { start(); };
  ~ScopedTimeLogger() { end(); };

 private:
  std::string mName;
  std::chrono::steady_clock::time_point mStart;
  std::chrono::steady_clock::time_point mEnd;
  std::chrono::milliseconds mDurationMs;

  void start() {
    mStart = std::chrono::steady_clock::now();
  }

  void end() {
    mEnd = std::chrono::steady_clock::now();
    mDurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(mEnd - mStart);
    LOG(INFO) << "[TIME_LOGGER] " << mName << " took " << mDurationMs.count() << " ms";
  }

};

} // namespace EaselPowerBlue
} // namespace android

#endif  // POWER_SCOPEDTIMELOGGER_H_
