#ifndef PAINTBOX_MANAGER_SERVER_H
#define PAINTBOX_MANAGER_SERVER_H

#include <binder/BinderService.h>

#include <thread>
#include <unordered_map>

#include "android/EaselManager/BnManagerService.h"
#include "android/EaselManager/IServiceStatusCallback.h"

#include "EaselComm2.h"

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

 private:
  int powerOn();
  void powerOff();

  std::mutex mManagerLock;
  std::unordered_map<int32_t, sp<IServiceStatusCallback>>
      mServiceCallbackMap;  // Guarded by mManagerLock;
  std::unique_ptr<EaselComm2::Comm> mComm;
  std::thread mCommOpenThread;

  // Initializes the binder server and easelcomm client.
  // Called in constructor because the object is statically initialized in
  // easelmanagerd.cpp.
  void initialize();
};

}  // namespace EaselManager
}  // namespace android

#endif  // PAINTBOX_MANAGER_SERVER_H
