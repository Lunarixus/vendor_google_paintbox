#ifndef PAINTBOX_LOG_BUFFER_EASEL_H
#define PAINTBOX_LOG_BUFFER_EASEL_H

#include <mutex>
#include <thread>

#include "easelcomm.h"

namespace EaselLog {

// States that LogClient could fall to.
enum class LogClientState {
  STARTING,  // LogClient is starting, but mCommClient is not opened.
  STARTED,  // mCommClient is fully started.
  STOPPING,  // mCommClient is going to be stopped.
  STOPPED,  // Default state, LogClient is stopped, mReceivingThread joined.
};

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
  // Condition when mCommClient is opened
  std::condition_variable mStarted;
  std::atomic<LogClientState> mState;
};

}  // namespace EaselLog

#endif  // PAINTBOX_LOG_BUFFER_EASEL_H