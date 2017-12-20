#define LOG_TAG "EaselManagerServer"

#include "ManagerServer.h"
#include "ManagerShared.h"

#include "android-base/logging.h"
#include "binder/IServiceManager.h"

#include "EaselManager.h"
#include "EaselManagerCommon.h"
#include "vendor/google_paintbox/easel/manager/shared/proto/easelmanager.pb.h"

namespace android {
namespace EaselManager {

const int kRetryTimes = 3;

namespace {
EaselManagerService::Service convertService(int32_t service) {
  Service serviceEnum = static_cast<Service>(service);
  switch (serviceEnum) {
    case PBSERVER:
      return EaselManagerService::PBSERVER;
    case DUMMY_SERVICE:
      return EaselManagerService::DUMMY_SERVICE;
    case CRASH_SERVICE:
      return EaselManagerService::CRASH_SERVICE;
    default:
      LOG(FATAL) << "Service " << service << " not defined";
  }
}

Service convertService(EaselManagerService::Service service) {
  switch (service) {
    case EaselManagerService::PBSERVER:
      return PBSERVER;
    case EaselManagerService::DUMMY_SERVICE:
      return DUMMY_SERVICE;
    case EaselManagerService::CRASH_SERVICE:
      return CRASH_SERVICE;
    default:
      LOG(FATAL) << "Service " << service << " not defined";
  }
}

Error convertError(EaselManagerService::Error error) {
  switch (error) {
    case EaselManagerService::SUCCESS:
      return SUCCESS;
    case EaselManagerService::SERVICE_ALREADY_STARTED:
      return SERVICE_ALREADY_STARTED;
    case EaselManagerService::SERVICE_NOT_FOUND:
      return SERVICE_NOT_FOUND;
    case EaselManagerService::SERVICE_PROCESS_FAILURE:
      return SERVICE_PROCESS_FAILURE;
    case EaselManagerService::SERVICE_NOT_STARTED:
      return SERVICE_NOT_STARTED;
    case EaselManagerService::EASEL_CONTROL_NO_INIT:
      return EASEL_CONTROL_NO_INIT;
    case EaselManagerService::EASEL_POWER_ON_ERROR:
      return EASEL_POWER_ON_ERROR;
    case EaselManagerService::EASEL_POWER_OFF_ERROR:
      return EASEL_POWER_OFF_ERROR;
    case EaselManagerService::EASEL_RESUME_ERROR:
      return EASEL_RESUME_ERROR;
    case EaselManagerService::EASEL_SUSPEND_ERROR:
      return EASEL_SUSPEND_ERROR;
    case EaselManagerService::EASEL_FATAL:
      return EASEL_FATAL;
    default:
      LOG(FATAL) << "Error " << error << " not defined";
  }
}

bool retryFunction(std::function<int()> func, int retryTimes = kRetryTimes) {
  for (int i = 0; i < retryTimes; ++i) {
    if (func() == 0) return true;
  }
  return false;
}

}  // namespace

ManagerServer::ManagerServer() : mEaselControlOpened(false) {
  int res = powerOn();
  if (res != 0) {
    LOG(ERROR) << __FUNCTION__ << ": Failed to power on Easel:"
               << strerror(-res);
  }
  initialize();
}

ManagerServer::~ManagerServer() {
  powerOff();
}

char const* ManagerServer::getServiceName() {
  return gEaselManagerService;
}

void ManagerServer::initialize() {
  LOG(INFO) << "Initialize easelcomm client.";

  mComm = EaselComm2::Comm::create(EaselComm2::Comm::Mode::CLIENT);
  mComm->registerHandler(SERVICE_STATUS,
                         [&](const EaselComm2::Message& message) {
    EaselManagerService::ServiceStatusResponse response;
    if (!message.toProto(&response)) {
      LOG(ERROR) << "Could not parse response.";
      return;
    }

    std::lock_guard<std::mutex> lock(mManagerLock);

    auto iter = mServiceInfoMap.find(convertService(response.service()));
    if (iter == mServiceInfoMap.end()) {
      LOG(ERROR) << "Could not find service " << response.service();
      return;
    }

    if (response.error() != EaselManagerService::SUCCESS) {
      iter->second.serviceCallback->onServiceError(
          convertError(response.error()));
      // Immediately clears the callback if happen occurs.
      // Client will not get any new updates about this service
      // Until new callback registers with startService call.
      mServiceInfoMap.erase(iter);
    } else {
      if (response.status() == EaselManagerService::LIVE) {
        LOG(INFO) << "Service " << response.service() << " started";
        iter->second.serviceCallback->onServiceStart();
      } else if (response.status() == EaselManagerService::EXIT) {
        LOG(INFO) << "Service " << response.service()
                  << " stopped, exit " <<  response.exit();
        iter->second.serviceCallback->onServiceEnd(response.exit());
        mServiceInfoMap.erase(iter);
      } else {
        LOG(FATAL) << "Service " << response.service()
                   << " unknown but no error reported";
      }
    }

    if (mServiceInfoMap.empty()) {
      LOG(INFO) << "All services quit, suspending";
      suspendInternal();
    }
  });

  // Open the channel.
  int res = retryFunction([&]() {
    return mComm->open(EASEL_SERVICE_MANAGER, /*timeout_ms=mm*/100);
  });
  if (!res) {
    LOG(ERROR) << __FUNCTION__ << ": Failed to open easelcomm channel.";
    return;
  }

  res = retryFunction([&]() { return mComm->startReceiving(); });
  if (!res) {
    LOG(ERROR) << __FUNCTION__
               << ": Failed to start easelcomm receiving thread.";
    return;
  }

  // Suspend Easel after initialization.
  res = suspendInternal();
  if (res != 0) {
    LOG(ERROR) << __FUNCTION__ << ": Failed to suspend Easel."
               << strerror(-res);
  }
}

binder::Status ManagerServer::startService(
    int32_t service,
    const sp<IServiceStatusCallback>& callback,
    int32_t* _aidl_return) {
  LOG(INFO) << __FUNCTION__ << ": Service " << service;

  std::lock_guard<std::mutex> lock(mManagerLock);

  auto iter = mServiceInfoMap.find(service);
  if (iter != mServiceInfoMap.end()) {
    resume(service, _aidl_return);
    return binder::Status::ok();
  }

  mServiceInfoMap[service] = {
    /*stateRequest=*/RESUME,
    /*serviceCallback=*/callback
  };

  EaselManagerService::StartServiceRequest request;
  request.set_service(convertService(service));

  if (!mComm->connected()) {
    resume(service, _aidl_return);
    if (*_aidl_return != static_cast<int32_t>(SUCCESS) &&
        *_aidl_return != static_cast<int32_t>(SERVICE_NOT_STARTED)) {
      return binder::Status::ok();
    }
  }

  mComm->send(START_SERVICE, request);

  *_aidl_return = static_cast<int32_t>(SUCCESS);

  return binder::Status::ok();
}

binder::Status ManagerServer::stopService(int32_t service,
                                          int32_t* _aidl_return) {
  LOG(INFO) << __FUNCTION__ << ": Service " << service;

  std::lock_guard<std::mutex> lock(mManagerLock);

  auto iter = mServiceInfoMap.find(service);
  if (iter != mServiceInfoMap.end()) {
    EaselManagerService::StopServiceRequest request;
    request.set_service(convertService(service));
    mComm->send(STOP_SERVICE, request);
    *_aidl_return = static_cast<int32_t>(SUCCESS);
  } else {
    *_aidl_return = static_cast<int32_t>(SERVICE_NOT_STARTED);
  }

  return binder::Status::ok();
}

int ManagerServer::powerOn() {
  LOG(INFO) << "Easel power on";

  Mutex::Autolock l(mEaselControlLock);
  int res = retryFunction([&]() { return mEaselControl.open(); });
  if (!res) {
    LOG(ERROR) << __FUNCTION__ << ": Failed to open Easel control: "
               << strerror(errno) << " (" << -errno << ").";
    return EASEL_FATAL;
  }
  mEaselControlOpened = true;

  mEaselControl.registerErrorCallback([&](enum EaselErrorReason reason,
                                          enum EaselErrorSeverity severity) {
    std::string errMsg;
    switch (severity) {
      case EaselErrorSeverity::FATAL:
        errMsg.append("Fatal: ");
        break;
      case EaselErrorSeverity::NON_FATAL:
        errMsg.append("Non-fatal: ");
        break;
      default:
        break;
    }

    switch (reason) {
      case EaselErrorReason::LINK_FAIL:
        errMsg.append("PCIe link down.");
        break;
      case EaselErrorReason::BOOTSTRAP_FAIL:
        errMsg.append("AP didn't receive bootstrap msi.");
        break;
      case EaselErrorReason::OPEN_SYSCTRL_FAIL:
        errMsg.append("AP failed to open SYSCTRL service.");
        break;
      case EaselErrorReason::HANDSHAKE_FAIL:
        errMsg.append("Handshake failed.");
        break;
      case EaselErrorReason::IPU_RESET_REQ:
        errMsg.append("Easel requested AP to reset it.");
        break;
      case EaselErrorReason::WATCHDOG:
        errMsg.append("Watchdog bite.");
        break;
      default:
        errMsg.append("Unknown error.");
        break;
    }

    LOG(ERROR) << __FUNCTION__ << ": Got an Easel error: " << errMsg.c_str();

    if (severity != EaselErrorSeverity::FATAL) {
      LOG(INFO) << __FUNCTION__ << ": Ignore non-fatal Easel error.";
      return 0;
    }

    notifyAllServicesFatal();

    return 0;
  });

  // TODO(b/70727332): remove "resume" call when libeaselcontrol has "powerOn".
  res = retryFunction([&]() { return mEaselControl.resume(); });
  if (!res) {
    LOG(ERROR) << __FUNCTION__ << ": Resume Easel failed: " << strerror(-res);
    return EASEL_RESUME_ERROR;
  }
  // "resume" is non-blocking, so sleep 1 second to make sure Easel is resumed.
  // This will go away when libeaselcontrol "blue" is implemented (b/70727332),
  // as it will be blocking.
  sleep(1);

  return SUCCESS;
}

void ManagerServer::powerOff() {
  mComm->close();

  LOG(INFO) << "Easel power off";
  Mutex::Autolock l(mEaselControlLock);
  if (mEaselControlOpened) {
    mEaselControl.close();
    mEaselControlOpened = false;
  }
}

int ManagerServer::suspendInternal() {
  LOG(INFO) << __FUNCTION__ << ": Suspending Easel.";

  Mutex::Autolock l(mEaselControlLock);
  if (!mEaselControlOpened) {
    LOG(ERROR) << __FUNCTION__ << ": Easel control is not opened.";
    return NO_INIT;
  }
  if (!mEaselResumed) {
    return 0;
  }

  int res = mEaselControl.suspend();
  if (res == 0) {
    mEaselResumed = false;
  }
  return res;
}

binder::Status ManagerServer::suspend(int32_t service, int32_t* _aidl_return) {
  Mutex::Autolock l(mEaselControlLock);
  if (!isServiceStarted(service)) {
    *_aidl_return = static_cast<int32_t>(SERVICE_NOT_STARTED);
    return binder::Status::ok();
  }

  mServiceInfoMap[service].stateRequest = SUSPEND;

  if (areAllServicesSuspend()) {
    int res = suspendInternal();
    if (res != 0) {
      LOG(ERROR) << __FUNCTION__ << ": Failed to suspend Easel."
                 << strerror(-res);
      *_aidl_return = static_cast<int32_t>(EASEL_SUSPEND_ERROR);
    } else {
      *_aidl_return = static_cast<int32_t>(SUCCESS);
    }
  } else {
    LOG(DEBUG) <<  __FUNCTION__
               << ": Waiting for other services to request SUSPEND.";
    *_aidl_return = static_cast<int32_t>(SUCCESS);
  }
  return binder::Status::ok();
}

binder::Status ManagerServer::resume(int32_t service, int32_t* _aidl_return) {
  LOG(DEBUG) << __FUNCTION__ << ": Resuming Easel.";

  Mutex::Autolock l(mEaselControlLock);
  if (!isServiceStarted(service)) {
    *_aidl_return = static_cast<int32_t>(SERVICE_NOT_STARTED);
    return binder::Status::ok();
  }

  if (!mEaselControlOpened) {
    LOG(ERROR) << __FUNCTION__ << ": Easel control is not opened.";
    *_aidl_return = static_cast<int32_t>(EASEL_CONTROL_NO_INIT);
    return binder::Status::ok();
  }

  int retCode = SUCCESS;
  if (mEaselResumed) {
    LOG(DEBUG) << __FUNCTION__ << ": Easel is already resumed.";
    mServiceInfoMap[service].stateRequest = RESUME;
  } else {
    int res = mEaselControl.resume();
    if (res != OK) {
      LOG(ERROR) << __FUNCTION__ << ": Resume Easel failed: " << strerror(-res);
      retCode = EASEL_RESUME_ERROR;
    } else {
      mServiceInfoMap[service].stateRequest = RESUME;
      mEaselResumed = true;
    }
  }
  *_aidl_return = static_cast<int32_t>(retCode);
  return binder::Status::ok();
}

bool ManagerServer::areAllServicesSuspend() {
  for (auto& s : mServiceInfoMap) {
    if (s.second.stateRequest == RESUME) {
      return false;
    }
  }
  return true;
}

bool ManagerServer::isServiceStarted(int32_t service) {
  auto iter = mServiceInfoMap.find(service);
  if (iter == mServiceInfoMap.end()) {
    LOG(DEBUG) << __FUNCTION__ << ": Service " << convertService(service)
               << " is not started on Easel.";
    return false;
  }
  return true;
}

void ManagerServer::notifyAllServicesFatal() {
  for (auto& service : mServiceInfoMap) {
    service.second.serviceCallback->onServiceError(
      convertError(EaselManagerService::EASEL_FATAL));
  }
}

}  // namespace EaselManager
}  // namespace android
