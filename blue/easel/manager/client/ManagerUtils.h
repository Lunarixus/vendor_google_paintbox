#ifndef PAINTBOX_MANAGER_UTILS_H
#define PAINTBOX_MANAGER_UTILS_H

namespace android {
namespace EaselManager {

const int kRetryTimes = 3;

// Attempts to run a function for at most "retryTimes" times.
// Returns true if function runs successfully, otherwise, return false.
inline bool retryFunction(std::function<int()> func,
                          int retryTimes = kRetryTimes) {
  for (int i = 0; i < retryTimes; ++i) {
    if (func() == 0) return true;
  }
  return false;
}

}
}

#endif  // PAINTBOX_MANAGER_UTILS_H
