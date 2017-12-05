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
  Error startService(Service service,
                     const sp<IServiceStatusCallback>& callback) override;
  Error stopService(Service service) override;

 private:
  sp<IManagerService> mService;
};

}  // namespace EaselManager
}  // namespace android

#endif  // PAINTBOX_MANAGER_CLIENT_IMPL_H
