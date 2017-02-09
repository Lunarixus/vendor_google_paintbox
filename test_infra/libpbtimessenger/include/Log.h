#ifndef EASEL_MESSENGER_LOG_H
#define EASEL_MESSENGER_LOG_H

#include <utils/Log.h>

/**
 * This header provides logging macros that can be used on AP (when simulating
 * an Easel process on AP) or on Easel depending on if ANDROID is defined. If
 * ANDROID is defined, original ALOG macros will be used. If ANDROID is not
 * defined, ALOG macros will be redefined as wrappers of easelLog that
 * transfers logs to the client on AP via EaselComm.
 */
#ifndef ANDROID
// If ANDROID is not defined, use Easel logging.
#include <easelcontrol.h>

// Undefine ALOG macros
#undef ALOGV
#undef ALOGD
#undef ALOGI
#undef ALOGW
#undef ALOGE

// Define ALOG macros as wrappers of easelLog.

// Enable verbose logging if LOG_NDEBUG is 0.
#if LOG_NDEBUG
#define ALOGV(...) do { if (0) { __ALOGV(__VA_ARGS__); } } while (0)
#else
#define ALOGV(...) (easelLog(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__))
#endif

#define ALOGD(...) (easelLog(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#define ALOGI(...) (easelLog(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#define ALOGW(...) (easelLog(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__))
#define ALOGE(...) (easelLog(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))

#else
// If ANDROID is defined, use original ALOG macros.
#endif  // ANDROID

#endif  // EASEL_MESSENGER_LOG_H
