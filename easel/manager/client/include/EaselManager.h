#ifndef PAINTBOX_EASEL_MANAGER_H
#define PAINTBOX_EASEL_MANAGER_H

#include "android/EaselManager/IAppStatusCallback.h"

namespace android {
namespace EaselManager {

// Supported apps on Easel.
enum App {
  PBSERVER = 1,
};

// Error codes
enum Error {
  SUCCESS = 0,
  ANDROID_SERVICE_MANAGER_ERROR = 1,  // Could not get IServiceManager.
  BINDER_ERROR = 2,                   // Binder transaction error.
  MANAGER_SERVICE_ERROR = 3,          // Could not get EaselManagerServer.
  APP_NOT_STARTED = 4,                // Could not started the app.
};

// EaselManager client.
class ManagerClient {
 public:
  ManagerClient(const ManagerClient&) = delete;
  ManagerClient& operator=(const ManagerClient&) = delete;

  virtual ~ManagerClient();

  // Creates the default instance of ManagerClient.
  static std::unique_ptr<ManagerClient> create();
  // Initializes the ManagerClient, returns the error code.
  virtual Error initialize() = 0;
  // Starts the app and registeres the callback, returns the error code.
  virtual Error startApp(App app, const sp<IAppStatusCallback>& callback) = 0;
  // Stops the app, returns the error code.
  virtual Error stopApp(App app) = 0;

 protected:
  ManagerClient();
};

}  // namespace EaselManager
}  // namespace android

#endif  // PAINTBOX_EASEL_MANAGER_H
