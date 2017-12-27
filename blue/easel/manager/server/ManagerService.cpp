#define LOG_TAG "EaselManagerService"

#include "ManagerService.h"

#include <sys/wait.h>
#include <fstream>
#include <thread>

#include "android-base/logging.h"

namespace EaselManagerService {

namespace {
std::string getServicePath(Service service) {
  switch (service) {
    case PBSERVER:
      return "/system/bin/pbserver";
    case DUMMY_SERVICE_1:
      return "/system/bin/easeldummyapp1";
    case DUMMY_SERVICE_2:
      return "/system/bin/easeldummyapp2";
    case CRASH_SERVICE:
      return "/system/bin/easelcrashapp";
    default:
      return "";
  }
}

ServiceStatusResponse getResponse(Service service,
                                  Error error,
                                  Status status, int exit) {
  ServiceStatusResponse response;
  response.set_service(service);
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
    std::function<void(const ServiceStatusResponse&)> statusCallback)
    : mStatusCallback(statusCallback) {}

void ManagerService::startService(const StartServiceRequest& request) {
  Service service = request.service();

  {
    std::lock_guard<std::mutex> lock(mServiceLock);
    if (mPidMap.find(service) != mPidMap.end()) {
      mStatusCallback(
          getResponse(service, SERVICE_ALREADY_STARTED, UNKNOWN, 0));
      return;
    }

    std::string servicePath = getServicePath(service);
    if (servicePath.empty() || !fileExist(servicePath.c_str())) {
      mStatusCallback(getResponse(service, SERVICE_NOT_FOUND, UNKNOWN, 0));
      return;
    }
  }

  LOG(INFO) << "Starting SERVICE " << service;

  pid_t pid = fork();

  if (pid == 0) {
    // Child process
    char* argv[] = {
      const_cast<char*>(getServicePath(service).c_str()), nullptr};
    int ret = execv(argv[0], argv);
    _exit(ret);
  } else if (pid > 0) {
    // Parent process
    std::lock_guard<std::mutex> lock(mServiceLock);
    mPidMap[service] = pid;
    mStatusCallback(getResponse(service, SUCCESS, LIVE, 0));

    // Create a new thread monitoring process state.
    std::thread([&, service, pid] {
      int exit = 0;
      // Block until child process ends.
      waitpid(pid, &exit, 0);
      LOG(INFO) << "service " << service
                << " pid (" << pid << ") terminates, exit " << exit;
      std::lock_guard<std::mutex> lock(mServiceLock);
      mPidMap.erase(service);
      mStatusCallback(getResponse(service, SUCCESS, EXIT, exit));
    })
        .detach();

    return;
  } else {
    mStatusCallback(getResponse(service, SERVICE_PROCESS_FAILURE, UNKNOWN, 0));
    return;
  }
}

void ManagerService::stopService(const StopServiceRequest& request) {
  Service service = request.service();

  std::lock_guard<std::mutex> lock(mServiceLock);

  auto iter = mPidMap.find(service);
  if (iter == mPidMap.end()) {
    mStatusCallback(getResponse(service, SERVICE_NOT_STARTED, UNKNOWN, 0));
    return;
  } else {
    LOG(INFO) << "Stopping SERVICE " << service;
    kill(iter->second, SIGTERM);
    // Callback will be sent at waitpid thread after termination.
  }
}

}  // namespace EaselManagerService
