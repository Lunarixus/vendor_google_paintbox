#ifndef PAINTBOX_MANAGER_SERVICE_H
#define PAINTBOX_MANAGER_SERVICE_H

#include <functional>
#include <mutex>
#include <unordered_map>

#include "vendor/google_paintbox/easel/manager/shared/proto/easelmanager.pb.h"

namespace EaselManagerService {

// App management implementation
class ManagerService {
 public:
  ManagerService(std::function<void(const AppStatusResponse&)> statusCallback);

  // Starts an app based on request and triggers statusCallback registered.
  void startApp(const StartAppRequest& request);
  // Stops an app based on request and triggers statusCallback registered.
  void stopApp(const StopAppRequest& request);

 private:
  std::mutex mServiceLock;
  // Pre-registered App status update callback function.
  std::function<void(const AppStatusResponse&)>
      mStatusCallback;                     // Guarded by mServiceLock
  std::unordered_map<App, pid_t> mPidMap;  // Guarded by mServiceLock
};

}  // namespace EaselManagerService

#endif  // PAINTBOX_MANAGER_SERVICE_H
