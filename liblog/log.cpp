// Implementation of logging on easel to replace the original liblog.so from
// Android toolchain. This implementaion will both print to stdout and
// Android logcat.

// Reference:
// https://cs.corp.google.com/android/system/core/liblog/logger_write.c

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include <string>
#include <vector>
#include <unordered_map>

#include <cutils/properties.h>
#include <log/log.h>
#include <tombstone.h>

#include "easelcontrol.h"

#define LOG_BUF_SIZE 1024
#define TIMESTAMP_BUF_SIZE 30
#define LOG_LEVEL_ENV "LOG_LEVEL"
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

static int getLogLevel() {
  char* log_level = std::getenv(LOG_LEVEL_ENV);
  if (log_level == nullptr) {
    return LOG_LEVEL_DEFAULT;
  }
  std::string level_string(log_level);
  if (PRIO_MAP.count(level_string) == 0) {
    return LOG_LEVEL_DEFAULT;
  }
  return PRIO_MAP[level_string];
}

static const int kLogLevel = getLogLevel();

static void getTimestamp(char* timestamp, size_t size) {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  auto* tm = localtime(&tv.tv_sec);

  char datetime[TIMESTAMP_BUF_SIZE];
  size_t s = strftime(datetime, TIMESTAMP_BUF_SIZE, "%m-%d %T", tm);
  if (s == 0) {
    datetime[0] = '\0';
  }
  snprintf(timestamp, size, "%s.%06ld", datetime, tv.tv_usec);
}

int __android_log_write(int prio, const char* tag, const char *text) {
  if (prio < kLogLevel) {
    return 0;
  }

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

  char buf[LOG_BUF_SIZE];
  // TODO(cjluo): Currently easel and AP timestamp syncing is not accurate.
  // Once the timesyncing is improved, we could remove the easel side
  // timestamp.
  snprintf(buf, LOG_BUF_SIZE, "EASEL: %s", text);
  EaselControlServer::log(prio, tag, buf);

  return strlen(text);
}

void __android_log_close() {}

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

int __android_log_buf_print(int bufID, int prio,
                            const char *tag,
                            const char *fmt, ...)
{
    va_list ap;
    char buf[LOG_BUF_SIZE];

    int len = snprintf(buf, LOG_BUF_SIZE, "buf id %d: ", bufID);
    if (len >= LOG_BUF_SIZE - len) {
      return 0;
    }

    va_start(ap, fmt);
    vsnprintf(buf + len, LOG_BUF_SIZE - len, fmt, ap);
    va_end(ap);

    return __android_log_write(prio, tag, buf);
}

// Block to register fatal signal handler to dumpstack trace.
// The default signal hander in linker64 requires crash_dump and tombstoned
// to be present and they are deeply integrated with Android implementation
// of logd. Here is the simplified implementation of generating the stack dump
// on Easel when program crashes.
// Reference:
// https://cs.corp.google.com/android/system/core/debuggerd/handler/debuggerd_handler.cpp

// Mutex to ensure only one crashing thread dumps itself.
static pthread_mutex_t crash_mutex = PTHREAD_MUTEX_INITIALIZER;

extern "C" void __linker_use_fallback_allocator();

static void signal_handler(int, siginfo_t* info, void* context) {
  int ret = pthread_mutex_lock(&crash_mutex);
  if (ret != 0) {
    __android_log_print(ANDROID_LOG_FATAL, "DEBUG", "pthread_mutex_lock failed: %s", strerror(ret));
    return;
  }

  ucontext_t* ucontext = static_cast<ucontext_t*>(context);
  engrave_tombstone_ucontext(-1, getpid(), gettid(), 0, info, ucontext);

  signal(info->si_signo, SIG_DFL);
  pthread_mutex_unlock(&crash_mutex);
}

struct RegisterSignalHandler {
  RegisterSignalHandler() {
    struct sigaction action = {};
    sigfillset(&action.sa_mask);
    action.sa_sigaction = signal_handler;
    action.sa_flags = SA_RESTART | SA_SIGINFO;

    // Use the alternate signal stack if available so we can catch stack overflows.
    action.sa_flags |= SA_ONSTACK;

    sigaction(SIGABRT, &action, nullptr);
    sigaction(SIGBUS, &action, nullptr);
    sigaction(SIGFPE, &action, nullptr);
    sigaction(SIGILL, &action, nullptr);
    sigaction(SIGSEGV, &action, nullptr);
#if defined(SIGSTKFLT)
    sigaction(SIGSTKFLT, &action, nullptr);
#endif
    sigaction(SIGSYS, &action, nullptr);
    sigaction(SIGTRAP, &action, nullptr);
  };
};

static RegisterSignalHandler register_signal_handler_block;

// Logs an log entry.
// Overrides weak symbol in libdebuggerd.
void _LOG(log_t*, logtype, const char *fmt, ...) {
  va_list ap;
  char buf[LOG_BUF_SIZE];

  va_start(ap, fmt);
  vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
  va_end(ap);

  __android_log_write(ANDROID_LOG_FATAL, "DEBUG", buf);
}

// Dummy implementation: workaround of library dependecy
// TODO(cjluo): Considering tunneling it to Android AP

int property_get(const char *, char *, const char *) {
  return 0;
}
