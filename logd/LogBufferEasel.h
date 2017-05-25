#ifndef PAINTBOX_LOG_BUFFER_EASEL_H
#define PAINTBOX_LOG_BUFFER_EASEL_H

#include "LogBufferInterface.h"

#include "easelcomm.h"

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
  const bool mLogToConsole;
};

#endif  // PAINTBOX_LOG_BUFFER_EASEL_H
