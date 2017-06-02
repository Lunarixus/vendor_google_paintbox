#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>
#include <unordered_map>

#include "log/log.h"

#include "LogBufferEasel.h"
#include "LogMessage.h"
#include "LogEntry.h"

#define LOG_LEVEL_ENV "LOG_LEVEL"
#define LOG_DEST_ENV "LOG_DEST"
#define LOG_FILE "LOG_FILE"
#define LOG_LEVEL_DEFAULT ANDROID_LOG_INFO

static std::vector<std::string> PRIO_LIST = {
  "UNKNOWN",
  "DEFAULT",
  "VERBOSE",
  "DEBUG",
  "INFO",
  "WARN",
  "ERROR",
  "FATAL",
  "SILENT"};

static std::unordered_map<std::string, int> PRIO_MAP = {
  {"UNKNOWN", ANDROID_LOG_UNKNOWN},
  {"DEFAULT", ANDROID_LOG_DEFAULT},
  {"VERBOSE", ANDROID_LOG_VERBOSE},
  {"DEBUG", ANDROID_LOG_DEBUG},
  {"INFO", ANDROID_LOG_INFO},
  {"WARN", ANDROID_LOG_WARN},
  {"ERROR", ANDROID_LOG_ERROR},
  {"FATAL", ANDROID_LOG_FATAL},
  {"SILENT", ANDROID_LOG_SILENT}};

static std::string getEnv(const char* env_name) {
  char* env_value = std::getenv(env_name);
  if (env_value == nullptr) {
    return "";
  }
  return std::string(env_value);
}

static int getLogLevel() {
  std::string level = getEnv(LOG_LEVEL_ENV);
  if (level.empty()) {
    return LOG_LEVEL_DEFAULT;
  }

  if (PRIO_MAP.count(level) == 0) {
    return LOG_LEVEL_DEFAULT;
  }
  return PRIO_MAP[level];
}

static LogDest getLogDest() {
  std::string dest = getEnv(LOG_DEST_ENV);
  if (dest == "CONSOLE") {
    return LogDest::CONSOLE;
  } else if (dest == "FILE") {
    return LogDest::FILE;
  } else {
    return LogDest::LOGCAT;
  }
}

LogBufferEasel::LogBufferEasel()
    : mLogLevel(getLogLevel()), mLogDest(getLogDest()) {
  std::string logFilePath = getEnv(LOG_FILE);
  if (mLogDest == LogDest::FILE && !logFilePath.empty()) {
    mLogFile = fopen(logFilePath.c_str(), "w+");
  }

  if (mLogDest == LogDest::LOGCAT) {
    mCommServer.open(EaselComm::EASEL_SERVICE_LOG);
  }
}

LogBufferEasel::~LogBufferEasel() {
  if (mLogDest == LogDest::LOGCAT) {
    mCommServer.close();
  }

  if (mLogFile != nullptr) {
    fclose(mLogFile);
  }
}

int LogBufferEasel::log(
      log_id_t log_id, log_time realtime, uid_t uid, pid_t pid, pid_t tid,
      const char* msg, unsigned short len) {
  if (msg == nullptr || len == 0) {
    return 0;
  }

  int prio = *msg;
  if (prio < mLogLevel) {
    return 0;
  }

  if (mLogDest == LogDest::LOGCAT) {
    LogMessage logMessage(log_id, realtime, uid, pid, tid, msg, len);

    EaselComm::EaselMessage easelMsg;
    easelMsg.message_buf = reinterpret_cast<void*>(&logMessage);
    easelMsg.message_buf_size = logMessage.size();
    easelMsg.dma_buf = 0;
    easelMsg.dma_buf_size = 0;
    int ret = mCommServer.sendMessage(&easelMsg);
    if (ret != 0) {
      fprintf(stderr, "Could not send log errno %d.\n", errno);
    }
  } else {
    LogEntry entry = parseEntry(msg, len);
    FILE *f;
    if (mLogDest == LogDest::FILE && mLogFile != nullptr) {
      f = mLogFile;
    } else {
      f = (prio >= ANDROID_LOG_ERROR) ? stderr : stdout;
    }
    fprintf(f, "%lu(us) <%s> PID %d TID %d %s %s\n",
        realtime.usec(), PRIO_LIST[prio].c_str(),
        pid, tid, entry.tag, entry.text);
    fflush(f);
  }
  return len;
}
