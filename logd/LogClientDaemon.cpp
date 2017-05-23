#include <condition_variable>
#include <mutex>

#include "log/log.h"

#include "LogClient.h"

// A daemon app to start log client.
// This daemon is included in userdebug build
// and could be used to receive log from easel
// if easel is resumed not by camera hal, e.g.
// sysfs.
int main() {
  EaselLog::LogClient logClient;
  int ret = logClient.start();
  ALOG_ASSERT(ret == 0);

  // Process is not going to stop. Waits infinitely.
  pause();
}
