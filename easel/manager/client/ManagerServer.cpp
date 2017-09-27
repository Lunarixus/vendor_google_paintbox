#define LOG_TAG "EaselManagerServer"

#include "ManagerServer.h"
#include "ManagerShared.h"

#include "android-base/logging.h"
#include "binder/IServiceManager.h"

#include "EaselManager.h"

namespace android {
namespace EaselManager {

ManagerServer::ManagerServer() {}

char const* ManagerServer::getServiceName() { return gEaselManagerService; }

binder::Status ManagerServer::startApp(int32_t app,
                                       const sp<IAppStatusCallback>& callback,
                                       int32_t* _aidl_return) {
  LOG(INFO) << __FUNCTION__ << ": App " << app;
  mAppCallbackMap[app] = callback;
  mAppCallbackMap[app]->onAppStart();
  // TODO(cjluo): start the app on Easel
  *_aidl_return = static_cast<int32_t>(SUCCESS);
  return binder::Status::ok();
}

binder::Status ManagerServer::stopApp(int32_t app, int32_t* _aidl_return) {
  LOG(INFO) << __FUNCTION__ << ": App " << app;
  auto iter = mAppCallbackMap.find(app);
  if (iter != mAppCallbackMap.end()) {
    mAppCallbackMap[app]->onAppEnd();
    *_aidl_return = static_cast<int32_t>(SUCCESS);
    mAppCallbackMap.erase(iter);
  } else {
    *_aidl_return = static_cast<int32_t>(APP_NOT_STARTED);
  }
  // TODO(cjluo): stop the app on Easel
  return binder::Status::ok();
}

}  // namespace EaselManager
}  // namespace android
