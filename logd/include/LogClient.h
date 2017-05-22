#ifndef PAINTBOX_LOG_BUFFER_EASEL_H
#define PAINTBOX_LOG_BUFFER_EASEL_H

#include <mutex>
#include <thread>

#include "easelcomm.h"

namespace EaselLog {

// Log client to receive easel side logs.
class LogClient {
 public:
  LogClient();
  ~LogClient();

  // Starts receiving logging from server side and print to logcat.
  // Returns -EBUSY if failed or 0 if successful.
  int start();
  // Stops receiving logging from server side.
  void stop();

 private:
  void log();

  EaselCommClient mCommClient;
  std::thread mReceivingThread;
  // Guards the mCommClient;
  std::mutex mClientGuard;
  std::atomic_bool mFinish;
};

}  // namespace EaselLog

#endif  // PAINTBOX_LOG_BUFFER_EASEL_H