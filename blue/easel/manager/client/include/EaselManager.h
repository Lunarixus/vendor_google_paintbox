#ifndef PAINTBOX_EASEL_MANAGER_H
#define PAINTBOX_EASEL_MANAGER_H

#include "android/EaselManager/IServiceStatusCallback.h"

namespace android {
namespace EaselManager {

// Supported services on Easel.
enum Service {
  PBSERVER = 1,

  // Test services starts from here.
  DUMMY_SERVICE_1 = 10000,
  DUMMY_SERVICE_2 = 10001,
  CRASH_SERVICE = 10002,
};

// Error codes
enum Error {
  SUCCESS = 0,
  ANDROID_SERVICE_MANAGER_ERROR = 1,  // Could not get IServiceManager.
  BINDER_ERROR = 2,                   // Binder transaction error.
  MANAGER_SERVICE_ERROR = 3,          // Could not get EaselManagerServer.
  SERVICE_ALREADY_STARTED = 4,        // App service already started.
  SERVICE_NOT_FOUND = 5,              // Easel could not find app service.
  SERVICE_PROCESS_FAILURE = 6,        // Could not start app service process.
  SERVICE_NOT_STARTED = 7,            // Could not started the app service.
  EASEL_POWER_ON_ERROR = 8,           // Could not poweron easel.
  EASEL_POWER_OFF_ERROR = 9,          // Could not poweroff easel.
  EASEL_RESUME_ERROR = 10,            // Could not resume easel.
  EASEL_SUSPEND_ERROR = 11,           // Could not suspend easel.
  EASEL_FATAL = 12,                   // Easel fatal errors.
  EASEL_CONTROL_NO_INIT = 13,         // Easel control not opened.
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

  // Starts the service and registeres the callback, returns the error code.
  virtual Error startService(Service service,
                             const sp<IServiceStatusCallback>& callback) = 0;

  // Stops the service, returns the error code.
  virtual Error stopService(Service service) = 0;

  // Requests to set Easel on suspend mode, returns zero for success or error
  // code for failure.
  virtual Error suspend(Service service) = 0;

  // Resumes Easel from suspend mode, returns zero for success or error code for
  // failure.
  virtual Error resume(Service service) = 0;

 protected:
  ManagerClient();
};

}  // namespace EaselManager
}  // namespace android

#endif  // PAINTBOX_EASEL_MANAGER_H
