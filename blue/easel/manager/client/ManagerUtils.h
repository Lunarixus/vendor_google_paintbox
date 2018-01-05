#ifndef PAINTBOX_MANAGER_UTILS_H
#define PAINTBOX_MANAGER_UTILS_H

namespace android {
namespace EaselManager {

const int kRetryTimes = 3;

// Attempts to run a function for at most "retryTimes" times.
// Returns error code of the function.
inline int retryFunction(std::function<int()> func,
                         int retryTimes = kRetryTimes) {
  int ret = 0;
  for (int i = 0; i < retryTimes; ++i) {
    ret = func();
    if (ret == 0) return ret;
  }
  return ret;
}

}
}

#endif  // PAINTBOX_MANAGER_UTILS_H
