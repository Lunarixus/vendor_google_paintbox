#include "ManagerService.h"

namespace EaselManagerService {

ManagerService::ManagerService(
    std::function<void(const AppStatusResponse&)> statusCallback)
    : mStatusCallback(statusCallback) {}

void ManagerService::startApp(const StartAppRequest& request) {
  // Mock implementation of starting an app.
  AppStatusResponse response;
  response.set_app(request.app());
  response.set_error(SUCCESS);
  response.set_status(LIVE);
  mStatusCallback(response);
}

void ManagerService::stopApp(const StopAppRequest& request) {
  // Mock implementation of stopping an app.
  AppStatusResponse response;
  response.set_app(request.app());
  response.set_error(SUCCESS);
  response.set_status(EXIT);
  mStatusCallback(response);
}

}  // namespace EaselManagerService
