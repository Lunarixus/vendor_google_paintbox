#include "ManagerClientImpl.h"
#include "ManagerShared.h"

#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>

namespace android {
namespace EaselManager {

Error ManagerClientImpl::initialize() {
  android::ProcessState::initWithDriver("/dev/vndbinder");
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

Error ManagerClientImpl::startService(Service service,
                                  const sp<IServiceStatusCallback>& callback) {
  int32_t res;
  binder::Status status =
      mService->startService(static_cast<int32_t>(service), callback, &res);
  if (!status.isOk()) return BINDER_ERROR;
  return static_cast<Error>(res);
}

Error ManagerClientImpl::stopService(Service service) {
  int32_t res;
  binder::Status status =
      mService->stopService(static_cast<int32_t>(service), &res);
  if (!status.isOk()) return BINDER_ERROR;
  return static_cast<Error>(res);
}

Error ManagerClientImpl::suspend(Service service) {
  int32_t res;
  binder::Status status =
      mService->suspend(static_cast<int32_t>(service), &res);
  if (!status.isOk()) return BINDER_ERROR;
  return static_cast<Error>(res);
}

Error ManagerClientImpl::resume(Service service) {
  int32_t res;
  binder::Status status =
      mService->resume(static_cast<int32_t>(service), &res);
  if (!status.isOk()) return BINDER_ERROR;
  return static_cast<Error>(res);
}

}  // namespace EaselManager
}  // namespace android
