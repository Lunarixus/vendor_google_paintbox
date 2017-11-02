#ifndef PAINTBOX_EASEL_MANAGER_H
#define PAINTBOX_EASEL_MANAGER_H

#include "android/EaselManager/IAppStatusCallback.h"

namespace android {
namespace EaselManager {

// Supported apps on Easel.
enum App {
  PBSERVER = 1,

  // Test apps starts from here.
  DUMMY_APP = 10000,
  CRASH_APP = 10001,
};

// Error codes
enum Error {
  SUCCESS = 0,
  ANDROID_SERVICE_MANAGER_ERROR = 1,  // Could not get IServiceManager.
  BINDER_ERROR = 2,                   // Binder transaction error.
  MANAGER_SERVICE_ERROR = 3,          // Could not get EaselManagerServer.
  APP_ALREADY_STARTED = 4,            // App already started.
  APP_NOT_FOUND = 5,                  // Easel could not find app.
  APP_PROCESS_FAILURE = 6,            // Could not start app process.
  APP_NOT_STARTED = 7,                // Could not started the app.
  EASEL_POWER_ERROR = 8,              // Could not poweron easel.
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
