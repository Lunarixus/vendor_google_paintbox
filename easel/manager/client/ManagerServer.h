#ifndef PAINTBOX_MANAGER_SERVER_H
#define PAINTBOX_MANAGER_SERVER_H

#include <binder/BinderService.h>

#include <thread>
#include <unordered_map>

#include "android/EaselManager/BnManagerService.h"
#include "android/EaselManager/IAppStatusCallback.h"

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
  binder::Status startApp(int32_t app, const sp<IAppStatusCallback>& callback,
                          int32_t* _aidl_return) override;
  binder::Status stopApp(int32_t app, int32_t* _aidl_return) override;

 private:
  int powerOn();
  void powerOff();

  std::mutex mManagerLock;
  std::unordered_map<int32_t, sp<IAppStatusCallback>>
      mAppCallbackMap;  // Guarded by mManagerLock;
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
