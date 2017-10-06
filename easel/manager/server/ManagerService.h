#ifndef PAINTBOX_MANAGER_SERVICE_H
#define PAINTBOX_MANAGER_SERVICE_H

#include <functional>

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
  // Pre-registered App status update callback function.
  std::function<void(const AppStatusResponse&)> mStatusCallback;
};

}  // namespace EaselManagerService

#endif  // PAINTBOX_MANAGER_SERVICE_H
