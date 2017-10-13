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
      AppStatusResponse response;
      response.set_app(app);
      response.set_error(APP_ALREADY_STARTED);
      response.set_status(UNKNOWN);
      mStatusCallback(response);
      return;
    }

    std::string appPath = getAppPath(app);
    if (appPath.empty() || !fileExist(appPath.c_str())) {
      AppStatusResponse response;
      response.set_app(app);
      response.set_error(APP_NOT_FOUND);
      response.set_status(UNKNOWN);
      mStatusCallback(response);
      return;
    }
  }

  LOG(INFO) << "Starting APP " << app;

  pid_t pid = fork();

  if (pid == 0) {
    // Child process
    char* argv[] = {const_cast<char*>(getAppPath(app).c_str()), nullptr};
    int ret = execv(argv[0], argv);
    exit(ret);
  } else if (pid > 0) {
    // Parent process
    std::lock_guard<std::mutex> lock(mServiceLock);
    mPidMap[app] = pid;
    AppStatusResponse response;
    response.set_app(app);
    response.set_error(SUCCESS);
    response.set_status(LIVE);
    mStatusCallback(response);

    // Create a new thread monitoring process state.
    std::thread([&, app, pid] {
      int exit = 0;
      // Block until child process ends.
      waitpid(pid, &exit, 0);
      LOG(INFO) << "app " << app << " pid (" << pid << ") terminates, exit "
                << exit;
      std::lock_guard<std::mutex> lock(mServiceLock);
      mPidMap.erase(app);
      AppStatusResponse response;
      response.set_app(app);
      response.set_error(SUCCESS);
      response.set_status(EXIT);
      mStatusCallback(response);
    })
        .detach();

    return;
  } else {
    std::lock_guard<std::mutex> lock(mServiceLock);
    AppStatusResponse response;
    response.set_app(app);
    response.set_error(APP_PROCESS_FAILURE);
    response.set_status(UNKNOWN);
    mStatusCallback(response);
    return;
  }
}

void ManagerService::stopApp(const StopAppRequest& request) {
  App app = request.app();

  std::lock_guard<std::mutex> lock(mServiceLock);

  auto iter = mPidMap.find(app);
  if (iter == mPidMap.end()) {
    AppStatusResponse response;
    response.set_app(app);
    response.set_error(APP_NOT_STARTED);
    response.set_status(UNKNOWN);
    mStatusCallback(response);
    return;
  } else {
    LOG(INFO) << "Stopping APP " << app;
    kill(iter->second, SIGTERM);
  }
}

}  // namespace EaselManagerService
