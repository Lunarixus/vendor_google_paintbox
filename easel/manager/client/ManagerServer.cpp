#define LOG_TAG "EaselManagerServer"

#include "ManagerServer.h"
#include "ManagerShared.h"

#include "android-base/logging.h"
#include "binder/IServiceManager.h"

#include "EaselManager.h"
#include "EaselManagerCommon.h"
#include "vendor/google_paintbox/easel/manager/shared/proto/easelmanager.pb.h"

namespace android {
namespace EaselManager {

namespace {
EaselManagerService::App convertApp(int32_t app) {
  App appEnum = static_cast<App>(app);
  switch (appEnum) {
    case PBSERVER:
      return EaselManagerService::PBSERVER;
    case DUMMY_APP:
      return EaselManagerService::DUMMY_APP;
    case CRASH_APP:
      return EaselManagerService::CRASH_APP;
    default:
      LOG(FATAL) << "App " << app << " not defined";
  }
}

App convertApp(EaselManagerService::App app) {
  switch (app) {
    case EaselManagerService::PBSERVER:
      return PBSERVER;
    case EaselManagerService::DUMMY_APP:
      return DUMMY_APP;
    case EaselManagerService::CRASH_APP:
      return CRASH_APP;
    default:
      LOG(FATAL) << "App " << app << " not defined";
  }
}

Error convertError(EaselManagerService::Error error) {
  switch (error) {
    case EaselManagerService::SUCCESS:
      return SUCCESS;
    case EaselManagerService::APP_ALREADY_STARTED:
      return APP_ALREADY_STARTED;
    case EaselManagerService::APP_NOT_FOUND:
      return APP_NOT_FOUND;
    case EaselManagerService::APP_PROCESS_FAILURE:
      return APP_PROCESS_FAILURE;
    case EaselManagerService::APP_NOT_STARTED:
      return APP_NOT_STARTED;
    default:
      LOG(FATAL) << "Error " << error << " not defined";
  }
}
}  // namespace

ManagerServer::ManagerServer() {
  mComm = EaselComm2::Comm::create(EaselComm2::Comm::Mode::CLIENT);
  initialize();
}

ManagerServer::~ManagerServer() {
  powerOff();
}

char const* ManagerServer::getServiceName() { return gEaselManagerService; }

void ManagerServer::initialize() {
  mComm->registerHandler(APP_STATUS, [&](const EaselComm2::Message& message) {
    EaselManagerService::AppStatusResponse response;
    if (!message.toProto(&response)) {
      LOG(ERROR) << "Could not parse response.";
      return;
    }

    std::lock_guard<std::mutex> lock(mManagerLock);

    auto iter = mAppCallbackMap.find(convertApp(response.app()));
    if (iter == mAppCallbackMap.end()) {
      LOG(ERROR) << "Could not find app " << response.app();
      return;
    }

    if (response.error() != EaselManagerService::SUCCESS) {
      iter->second->onAppError(convertError(response.error()));
      // Immediately clears the callback if happen occurs.
      // Client will not get any new updates about this app
      // Until new callback registers with startApp call.
      mAppCallbackMap.erase(iter);
      return;
    }

    if (response.status() == EaselManagerService::LIVE) {
      iter->second->onAppStart();
    } else if (response.status() == EaselManagerService::EXIT) {
      iter->second->onAppEnd(response.exit());
      mAppCallbackMap.erase(iter);
    } else {
      LOG(FATAL) << "App " << response.app()
                 << " unknown but no error reported";
    }

    if (mAppCallbackMap.empty()) {
      powerOff();
    }
  });
}

binder::Status ManagerServer::startApp(int32_t app,
                                       const sp<IAppStatusCallback>& callback,
                                       int32_t* _aidl_return) {
  LOG(INFO) << __FUNCTION__ << ": App " << app;

  std::lock_guard<std::mutex> lock(mManagerLock);

  auto iter = mAppCallbackMap.find(app);
  if (iter != mAppCallbackMap.end()) {
    *_aidl_return = static_cast<int32_t>(APP_ALREADY_STARTED);
    return binder::Status::ok();
  }

  mAppCallbackMap[app] = callback;

  EaselManagerService::StartAppRequest request;
  request.set_app(convertApp(app));

  if (!mComm->connected()) {
    int res = powerOn();
    if (res != 0) {
      *_aidl_return = static_cast<int32_t>(EASEL_POWER_ERROR);
      return binder::Status::ok();
    }
  }

  mComm->send(START_APP, request);

  *_aidl_return = static_cast<int32_t>(SUCCESS);

  return binder::Status::ok();
}

binder::Status ManagerServer::stopApp(int32_t app, int32_t* _aidl_return) {
  LOG(INFO) << __FUNCTION__ << ": App " << app;

  std::lock_guard<std::mutex> lock(mManagerLock);

  auto iter = mAppCallbackMap.find(app);
  if (iter != mAppCallbackMap.end()) {
    EaselManagerService::StopAppRequest request;
    request.set_app(convertApp(app));
    mComm->send(STOP_APP, request);
    *_aidl_return = static_cast<int32_t>(SUCCESS);
  } else {
    *_aidl_return = static_cast<int32_t>(APP_NOT_STARTED);
  }

  return binder::Status::ok();
}

int ManagerServer::powerOn() {
  LOG(INFO) << "Easel power on";
  // TODO(b/68394081): Implement this method.

  // Open the channel without timeout to avoid busy polling.
  int res = mComm->open(EASEL_SERVICE_MANAGER);
  if (res != 0) return res;
  return mComm->startReceiving();
}

void ManagerServer::powerOff() {
  mComm->close();
  LOG(INFO) << "Easel power off";
  // TODO(b/68394081): Implement this method.
}

}  // namespace EaselManager
}  // namespace android
