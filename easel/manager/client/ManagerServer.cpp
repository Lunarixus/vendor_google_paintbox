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
    default:
      LOG(FATAL) << "App " << app << " not defined";
  }
}

App convertApp(EaselManagerService::App app) {
  switch (app) {
    case EaselManagerService::PBSERVER:
      return PBSERVER;
    default:
      LOG(FATAL) << "App " << app << " not defined";
  }
}

Error convertError(EaselManagerService::Error error) {
  switch (error) {
    case EaselManagerService::SUCCESS:
      return SUCCESS;
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
  mComm->close();
  mCommOpenThread.join();
}

char const* ManagerServer::getServiceName() { return gEaselManagerService; }

void ManagerServer::initialize() {
  mComm->registerHandler(APP_STATUS, [&](const EaselComm2::Message& message) {
    EaselManagerService::AppStatusResponse response;
    if (!message.toProto(&response)) {
      LOG(ERROR) << "Could not parse response.";
      return;
    }

    std::lock_guard<std::mutex> lock(mMapLock);

    auto iter = mAppCallbackMap.find(convertApp(response.app()));
    if (iter == mAppCallbackMap.end()) {
      LOG(ERROR) << "Could not find app " << response.app();
      return;
    }

    if (response.error() != EaselManagerService::SUCCESS) {
      iter->second->onAppError(convertError(response.error()));
      return;
    };

    if (response.status() == EaselManagerService::LIVE) {
      iter->second->onAppStart();
    } else if (response.status() == EaselManagerService::EXIT) {
      iter->second->onAppEnd();
      mAppCallbackMap.erase(iter);
    }
  });

  // Constantly try to connect to the easelmanagerserver on Easel side.
  mCommOpenThread = std::thread([&] {
    mComm->openPersistent(EASEL_SERVICE_MANAGER);
  });
}

binder::Status ManagerServer::startApp(int32_t app,
                                       const sp<IAppStatusCallback>& callback,
                                       int32_t* _aidl_return) {
  LOG(INFO) << __FUNCTION__ << ": App " << app;

  std::lock_guard<std::mutex> lock(mMapLock);
  mAppCallbackMap[app] = callback;

  EaselManagerService::StartAppRequest request;
  request.set_app(convertApp(app));
  mComm->send(START_APP, request);

  *_aidl_return = static_cast<int32_t>(SUCCESS);

  return binder::Status::ok();
}

binder::Status ManagerServer::stopApp(int32_t app, int32_t* _aidl_return) {
  LOG(INFO) << __FUNCTION__ << ": App " << app;

  std::lock_guard<std::mutex> lock(mMapLock);

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

}  // namespace EaselManager
}  // namespace android
