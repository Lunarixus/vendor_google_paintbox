#define LOG_TAG "EaselManagerServer"

#include "ManagerServer.h"

#include "android-base/logging.h"

#include "EaselManagerCommon.h"
#include "vendor/google_paintbox/blue/easel/manager/shared/proto/easelmanager.pb.h"

namespace EaselManagerService {

ManagerServer::ManagerServer() {
  mComm = std::unique_ptr<easel::Comm>(
      easel::Comm::Create(easel::Comm::Type::SERVER));

  mService = std::make_unique<ManagerService>(
      [&](ServiceStatusResponse response) {
    easel::SendProto(mComm.get(), SERVICE_STATUS, response, /*payload=*/nullptr);
  });

  mStartServiceStatusHandler = std::make_unique<easel::FunctionHandler>(
      [&](const easel::Message& message) {
    StartServiceRequest request;
    CHECK(easel::MessageToProto(message, &request));
    mService->startService(request);
  });

  mStopServiceStatusHandler = std::make_unique<easel::FunctionHandler>(
      [&](const easel::Message& message) {
    StopServiceRequest request;
    CHECK(easel::MessageToProto(message, &request));
    mService->stopService(request);
  });
}

void ManagerServer::run() {
  mComm->RegisterHandler(START_SERVICE, mStartServiceStatusHandler.get());
  mComm->RegisterHandler(STOP_SERVICE, mStopServiceStatusHandler.get());
  mComm->OpenPersistent(easel::EASEL_SERVICE_MANAGER, 0);
}
}  // namespace EaselManagerService
