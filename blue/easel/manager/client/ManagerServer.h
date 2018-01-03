#ifndef PAINTBOX_MANAGER_SERVER_H
#define PAINTBOX_MANAGER_SERVER_H

#include <binder/BinderService.h>

#include <utils/Mutex.h>
#include <thread>
#include <unordered_map>

#include "android/EaselManager/BnManagerService.h"
#include "android/EaselManager/IServiceStatusCallback.h"

#include "EaselComm2.h"
#include "control/ManagerControlClient.h"

namespace android {
namespace EaselManager {

// Server side implementation for easelmanagerd.
class ManagerServer : public BinderService<ManagerServer>,
                      public BnManagerService {
 public:
  ManagerServer();
  ~ManagerServer();
  static char const* getServiceName();
  binder::Status startService(int32_t service,
                              const sp<IServiceStatusCallback>& callback,
                              int32_t* _aidl_return) override;
  binder::Status stopService(int32_t service, int32_t* _aidl_return) override;
  binder::Status suspend(int32_t service, int32_t* _aidl_return) override;
  binder::Status resume(int32_t service, int32_t* _aidl_return) override;

 private:
  int powerOn();
  void powerOff();

  // Initializes the binder server and easelcomm client.
  // Called in constructor because the object is statically initialized in
  // easelmanagerd.cpp.
  void initialize();

  bool isServiceStarted(int32_t service);

  // Return true if all services requested to set Easel to suspend mode.
  bool areAllServicesSuspend();

  // Notify all services that Easel has fatal error.
  void notifyAllServicesFatal();

  enum EaselStateControlRequest {
    SUSPEND = 1,
    RESUME = 2,
  };

  struct ServiceInfo {
    EaselStateControlRequest stateRequest;
    sp<IServiceStatusCallback> serviceCallback;
  };

  std::mutex mManagerLock;
  std::unordered_map<int32_t, ServiceInfo>
      mServiceInfoMap;  // Guarded by mManagerLock;
  std::unique_ptr<EaselComm2::Comm> mComm;
  // If Easel is resumed. Protected by mManagerLock.
  bool mEaselResumed;
  // Easel control client. Protected by mManagerLock.
  std::unique_ptr<ManagerControlClient> mManagerControl;
};

}  // namespace EaselManager
}  // namespace android

#endif  // PAINTBOX_MANAGER_SERVER_H
