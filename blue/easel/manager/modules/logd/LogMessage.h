#ifndef PAINTBOX_LOG_MESSAGE_H
#define PAINTBOX_LOG_MESSAGE_H

#include <log/log_read.h>

// LogMessage contains all the information of a log entry.
struct LogMessage {
  const log_id_t log_id;
  const log_time realtime;
  const uid_t uid;
  const pid_t pid;
  const pid_t tid;
  const unsigned short len;
  char log[LOGGER_ENTRY_MAX_PAYLOAD];

  LogMessage(log_id_t log_id, log_time realtime, uid_t uid, pid_t pid, pid_t tid,
      const char* msg, unsigned short len)
      : log_id(log_id), realtime(realtime), uid(uid), pid(pid), tid(tid), len(len) {
    memcpy(log, msg, len);
  }

  // Returns the size of the message, truncating the log text string.
  size_t size() const {
    return sizeof(LogMessage) - LOGGER_ENTRY_MAX_PAYLOAD + len;
  }
};

#endif  // PAINTBOX_LOG_MESSAGE_H
