#define LOG_TAG "EaselManagerServcer"

#include "ManagerServer.h"

#include "android-base/logging.h"

#include "EaselManagerCommon.h"
#include "EaselService.h"
#include "vendor/google_paintbox/easel/manager/shared/proto/easelmanager.pb.h"

namespace EaselManagerService {

ManagerServer::ManagerServer() {
  mComm = EaselComm2::Comm::create(EaselComm2::Comm::Mode::SERVER);
  mService = std::make_unique<ManagerService>(
      [&](AppStatusResponse response) { mComm->send(APP_STATUS, response); });
}

void ManagerServer::run() {
  mComm->registerHandler(START_APP, [&](const EaselComm2::Message& message) {
    StartAppRequest request;
    CHECK(message.toProto(&request));
    mService->startApp(request);
  });

  mComm->registerHandler(STOP_APP, [&](const EaselComm2::Message& message) {
    StopAppRequest request;
    CHECK(message.toProto(&request));
    mService->stopApp(request);
  });

  mComm->openPersistent(EASEL_SERVICE_MANAGER);
}
}  // namespace EaselManagerService
