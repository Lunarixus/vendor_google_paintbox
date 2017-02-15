// Implementation of logging on easel to replace the original liblog.so from
// Android toolchain. This implementaion will both print to stdout and
// Android logcat.

// Reference:
// https://cs.corp.google.com/android/system/core/liblog/logger_write.c

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include <string>
#include <vector>

#include <android/log.h>

#include "easelcontrol.h"

#define LOG_BUF_SIZE 1024
#define TIMESTAMP_BUF_SIZE 30

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

static void getTimestamp(char* timestamp, size_t size) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  auto* tm = localtime(&tv.tv_sec);

  char datetime[TIMESTAMP_BUF_SIZE];
  size_t s = strftime(datetime, TIMESTAMP_BUF_SIZE, "%m-%d %T", tm);
  if (s == 0) {
    datetime[0] = '\0';
  }
  snprintf(timestamp, size, "%s.%06ld", datetime, tv.tv_usec);
}

int __android_log_write(int prio, const char* tag, const char *text) {
  if (tag == nullptr || text == nullptr) {
    return 0;
  }

  if (prio < 0 || prio >= (int)PRIO_LIST.size()) {
    prio = ANDROID_LOG_UNKNOWN;
  }
  char timestamp[TIMESTAMP_BUF_SIZE];
  getTimestamp(timestamp, TIMESTAMP_BUF_SIZE);
  // Prints to stdout.
  printf("%s  <%s> %s: %s\n", timestamp, PRIO_LIST[prio].c_str(), tag, text);
  // Logs to AP logcat.
  char buf[LOG_BUF_SIZE];
  // TODO(cjluo): Currently easel and AP timestamp syncing is not accurate.
  // Once the timesyncing is improved, we could remove the easel side
  // timestamp.
  snprintf(buf, LOG_BUF_SIZE, "[EASEL %s] %s", timestamp, text);
  EaselControlServer::log(prio, tag, buf);
  return strlen(text);
}

int __android_log_close() {
  return 0;
}

int __android_log_print(int prio, const char *tag,  const char *fmt, ...) {
  va_list ap;
  char buf[LOG_BUF_SIZE];

  va_start(ap, fmt);
  vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
  va_end(ap);

  return __android_log_write(prio, tag, buf);
}

int __android_log_vprint(int prio, const char *tag,
                         const char *fmt, va_list ap) {
  char buf[LOG_BUF_SIZE];

  vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);

  return __android_log_write(prio, tag, buf);
}

void __android_log_assert(const char *cond, const char *tag,
        const char *fmt, ...) {
  char buf[LOG_BUF_SIZE];

  if (fmt) {
      va_list ap;
      va_start(ap, fmt);
      vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
      va_end(ap);
  } else {
      // Msg not provided, log condition.  N.B. Do not use cond directly as
      // format string as it could contain spurious '%' syntax (e.g.
      // "%d" in "blocks%devs == 0").
      if (cond)
          snprintf(buf, LOG_BUF_SIZE, "Assertion failed: %s", cond);
      else
          strcpy(buf, "Unspecified assertion failed");
  }

  __android_log_write(ANDROID_LOG_FATAL, tag, buf);
  abort(); // abort so we have a chance to debug the situation
  // NOTREACHED
}