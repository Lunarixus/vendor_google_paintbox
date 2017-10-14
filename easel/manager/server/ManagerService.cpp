#define LOG_TAG "EaselManagerService"

#include "ManagerService.h"

#include <sys/wait.h>
#include <fstream>
#include <thread>

#include "android-base/logging.h"

namespace EaselManagerService {

namespace {
std::string getAppPath(App app) {
  switch (app) {
    case PBSERVER:
      return "/system/bin/pbserver";
    case DUMMY_APP:
      return "/system/bin/easeldummyapp";
    case CRASH_APP:
      return "/system/bin/easelcrashapp";
    default:
      return "";
  }
}

AppStatusResponse getResponse(App app, Error error, Status status, int exit) {
  AppStatusResponse response;
  response.set_app(app);
  response.set_error(error);
  response.set_status(status);
  response.set_exit(exit);
  return response;
}

bool fileExist(const char* path) {
  std::ifstream f(path);
  return f.good();
}
}  // namespace

ManagerService::ManagerService(
    std::function<void(const AppStatusResponse&)> statusCallback)
    : mStatusCallback(statusCallback) {}

void ManagerService::startApp(const StartAppRequest& request) {
  App app = request.app();

  {
    std::lock_guard<std::mutex> lock(mServiceLock);
    if (mPidMap.find(app) != mPidMap.end()) {
      mStatusCallback(getResponse(app, APP_ALREADY_STARTED, UNKNOWN, 0));
      return;
    }

    std::string appPath = getAppPath(app);
    if (appPath.empty() || !fileExist(appPath.c_str())) {
      mStatusCallback(getResponse(app, APP_NOT_FOUND, UNKNOWN, 0));
      return;
    }
  }

  LOG(INFO) << "Starting APP " << app;

  pid_t pid = fork();

  if (pid == 0) {
    // Child process
    char* argv[] = {const_cast<char*>(getAppPath(app).c_str()), nullptr};
    int ret = execv(argv[0], argv);
    _exit(ret);
  } else if (pid > 0) {
    // Parent process
    std::lock_guard<std::mutex> lock(mServiceLock);
    mPidMap[app] = pid;
    mStatusCallback(getResponse(app, SUCCESS, LIVE, 0));

    // Create a new thread monitoring process state.
    std::thread([&, app, pid] {
      int exit = 0;
      // Block until child process ends.
      waitpid(pid, &exit, 0);
      LOG(INFO) << "app " << app << " pid (" << pid << ") terminates, exit "
                << exit;
      std::lock_guard<std::mutex> lock(mServiceLock);
      mPidMap.erase(app);
      mStatusCallback(getResponse(app, SUCCESS, EXIT, exit));
    })
        .detach();

    return;
  } else {
    mStatusCallback(getResponse(app, APP_PROCESS_FAILURE, UNKNOWN, 0));
    return;
  }
}

void ManagerService::stopApp(const StopAppRequest& request) {
  App app = request.app();

  std::lock_guard<std::mutex> lock(mServiceLock);

  auto iter = mPidMap.find(app);
  if (iter == mPidMap.end()) {
    mStatusCallback(getResponse(app, APP_NOT_STARTED, UNKNOWN, 0));
    return;
  } else {
    LOG(INFO) << "Stopping APP " << app;
    kill(iter->second, SIGTERM);
    // Callback will be sent at waitpid thread after termination.
  }
}

}  // namespace EaselManagerService
