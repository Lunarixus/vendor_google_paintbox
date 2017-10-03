#ifndef PAINTBOX_MANAGER_SERVER_H
#define PAINTBOX_MANAGER_SERVER_H

#include <unordered_map>

#include <binder/BinderService.h>

#include "android/EaselManager/BnManagerService.h"
#include "android/EaselManager/IAppStatusCallback.h"

namespace android {
namespace EaselManager {

// Server side implementation for easelmanagerd.
class ManagerServer : public BinderService<ManagerServer>,
                      public BnManagerService {
 public:
  ManagerServer();
  static char const* getServiceName();
  binder::Status startApp(int32_t app, const sp<IAppStatusCallback>& callback,
                          int32_t* _aidl_return) override;
  binder::Status stopApp(int32_t app, int32_t* _aidl_return) override;

 private:
  std::unordered_map<int32_t, sp<IAppStatusCallback>> mAppCallbackMap;
};

}  // namespace EaselManager
}  // namespace android

#endif  // PAINTBOX_MANAGER_SERVER_H
