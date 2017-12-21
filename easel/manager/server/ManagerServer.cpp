#define LOG_TAG "EaselManagerServer"

#include "ManagerServer.h"

#include "android-base/logging.h"

#include "EaselManagerCommon.h"
#include "EaselService.h"
#include "vendor/google_paintbox/easel/manager/shared/proto/easelmanager.pb.h"

namespace EaselManagerService {

ManagerServer::ManagerServer() {
  mComm = EaselComm2::Comm::create(EaselComm2::Comm::Mode::SERVER);
  mService = std::make_unique<ManagerService>(
      [&](ServiceStatusResponse response) {
    mComm->send(SERVICE_STATUS, response);
  });
}

void ManagerServer::run() {
  mComm->registerHandler(START_SERVICE,
                         [&](const EaselComm2::Message& message) {
    StartServiceRequest request;
    CHECK(message.toProto(&request));
    mService->startService(request);
  });

  mComm->registerHandler(STOP_SERVICE, [&](const EaselComm2::Message& message) {
    StopServiceRequest request;
    CHECK(message.toProto(&request));
    mService->stopService(request);
  });

  mComm->openPersistent(EASEL_SERVICE_MANAGER);
}
}  // namespace EaselManagerService
