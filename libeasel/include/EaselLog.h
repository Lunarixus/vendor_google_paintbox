#ifndef __EASEL_LOG_H__
#define __EASEL_LOG_H__

#include <log/log.h>

#if LOG_NDEBUG
#define LOGV(...) do {} while(0)
#else
#define LOGV(...) do { \
    easelLog(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__); \
  } while(0)
#endif

#define LOGD(...) do { \
    easelLog(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__); \
  } while(0)

#define LOGI(...) do { \
    easelLog(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__); \
  } while(0)

#define LOGW(...) do { \
    easelLog(ANDROID_LOG_WARN,, LOG_TAG, __VA_ARGS__); \
  } while(0)

#define LOGE(...) do { \
    easelLog(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__); \
  } while(0)

#endif  // __EASEL_LOG_H__