#include "ManagerClientImpl.h"
#include "ManagerShared.h"

#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>

namespace android {
namespace EaselManager {

Error ManagerClientImpl::initialize() {
  sp<IServiceManager> sm = defaultServiceManager();
  if (sm == nullptr) return ANDROID_SERVICE_MANAGER_ERROR;
  sp<IBinder> binder = sm->getService(String16(gEaselManagerService));
  if (binder == nullptr) return BINDER_ERROR;
  mService = interface_cast<IManagerService>(binder);
  if (mService == nullptr) return MANAGER_SERVICE_ERROR;
  // Start the binder thread pool for callbacks.
  ProcessState::self()->startThreadPool();
  return SUCCESS;
}

Error ManagerClientImpl::startApp(App app,
                                  const sp<IAppStatusCallback>& callback) {
  int32_t res;
  binder::Status status =
      mService->startApp(static_cast<int32_t>(app), callback, &res);
  if (!status.isOk()) return BINDER_ERROR;
  return static_cast<Error>(res);
}

Error ManagerClientImpl::stopApp(App app) {
  int32_t res;
  binder::Status status = mService->stopApp(static_cast<int32_t>(app), &res);
  if (!status.isOk()) return BINDER_ERROR;
  return static_cast<Error>(res);
}

}  // namespace EaselManager
}  // namespace android
