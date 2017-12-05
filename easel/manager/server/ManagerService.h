#ifndef PAINTBOX_MANAGER_SERVICE_H
#define PAINTBOX_MANAGER_SERVICE_H

#include <functional>
#include <mutex>
#include <unordered_map>

#include "vendor/google_paintbox/easel/manager/shared/proto/easelmanager.pb.h"

namespace EaselManagerService {

// Service management implementation
class ManagerService {
 public:
  ManagerService(
    std::function<void(const ServiceStatusResponse&)> statusCallback);

  // Starts a service based on request and triggers statusCallback registered.
  void startService(const StartServiceRequest& request);
  // Stops a service based on request and triggers statusCallback registered.
  void stopService(const StopServiceRequest& request);

 private:
  std::mutex mServiceLock;
  // Pre-registered service status update callback function.
  std::function<void(const ServiceStatusResponse&)>
      mStatusCallback;                     // Guarded by mServiceLock
  std::unordered_map<Service, pid_t> mPidMap;  // Guarded by mServiceLock
};

}  // namespace EaselManagerService

#endif  // PAINTBOX_MANAGER_SERVICE_H
