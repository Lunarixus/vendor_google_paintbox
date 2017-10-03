#ifndef PAINTBOX_MANAGER_CLIENT_IMPL_H
#define PAINTBOX_MANAGER_CLIENT_IMPL_H

#include "EaselManager.h"

#include "android/EaselManager/IManagerService.h"

namespace android {
namespace EaselManager {

// Implementation of ManagerClient
class ManagerClientImpl : public ManagerClient {
 public:
  Error initialize() override;
  Error startApp(App app, const sp<IAppStatusCallback>& callback) override;
  Error stopApp(App app) override;

 private:
  sp<IManagerService> mService;
};

}  // namespace EaselManager
}  // namespace android

#endif  // PAINTBOX_MANAGER_CLIENT_IMPL_H
