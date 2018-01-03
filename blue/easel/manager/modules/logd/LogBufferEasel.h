#ifndef PAINTBOX_LOG_BUFFER_EASEL_H
#define PAINTBOX_LOG_BUFFER_EASEL_H

#include "LogBufferInterface.h"

#include "easelcomm.h"

// Destination about logging
enum class LogDest {
  LOGCAT,   // Log to Android logcat through PCIE.
  CONSOLE,  // Log to console.
  FILE,     // Log to file specified by LOG_FILE environment variable.
};

// LogBuffer implementation through easel PCIE.
class LogBufferEasel final : public LogBufferInterface {
 public:
  explicit LogBufferEasel();
  ~LogBufferEasel();
  int log(
      log_id_t log_id, log_time realtime, uid_t uid, pid_t pid, pid_t tid,
      const char* msg, unsigned short len) override;
 private:
  EaselCommServer mCommServer;
  const int mLogLevel;
  LogDest mLogDest;
  FILE* mLogFile;
};

#endif  // PAINTBOX_LOG_BUFFER_EASEL_H
