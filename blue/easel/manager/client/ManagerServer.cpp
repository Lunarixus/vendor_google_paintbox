#define LOG_TAG "EaselManagerServer"

#include "ManagerServer.h"
#include "ManagerShared.h"
#include "ManagerUtils.h"

#include "android-base/logging.h"
#include "binder/IServiceManager.h"

#include "EaselManager.h"
#include "EaselManagerCommon.h"
#include "control/ManagerControlClient.h"
#include "vendor/google_paintbox/blue/easel/manager/shared/proto/easelmanager.pb.h"

namespace android {
namespace EaselManager {

namespace {
EaselManagerService::Service convertService(int32_t service) {
  Service serviceEnum = static_cast<Service>(service);
  switch (serviceEnum) {
    case PBSERVER:
      return EaselManagerService::PBSERVER;
    case DUMMY_SERVICE_1:
      return EaselManagerService::DUMMY_SERVICE_1;
    case DUMMY_SERVICE_2:
      return EaselManagerService::DUMMY_SERVICE_2;
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
    case EaselManagerService::DUMMY_SERVICE_1:
      return DUMMY_SERVICE_1;
    case EaselManagerService::DUMMY_SERVICE_2:
      return DUMMY_SERVICE_2;
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

}  // namespace

ManagerServer::ManagerServer() {
  int res = powerOn();
  if (res != 0) {
    LOG(ERROR) << __FUNCTION__ << ": Failed to power on Easel:"
               << strerror(-res);
  }
  initialize();
  // Suspend Easel after initialization.
  mManagerControl->suspend();
}

ManagerServer::~ManagerServer() {
  powerOff();
}

char const* ManagerServer::getServiceName() {
  return gEaselManagerService;
}

void ManagerServer::initialize() {
  LOG(DEBUG) << __FUNCTION__ << "Initialize easelcomm client.";

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
      mManagerControl->suspend();
    }
  });

  // Open the channel.
  int res = retryFunction([&]() {
    return mComm->open(EASEL_SERVICE_MANAGER, /*timeout_ms=mm*/100);
  });
  if (!res) {
    LOG(ERROR) << __FUNCTION__ << ": Failed to open easelcomm channel:"
               << strerror(-res);
    return;
  }

  res = retryFunction([&]() { return mComm->startReceiving(); });
  if (!res) {
    LOG(ERROR) << __FUNCTION__
               << ": Failed to start easelcomm receiving thread:"
               << strerror(-res);
    return;
  }
}

binder::Status ManagerServer::startService(
    int32_t service,
    const sp<IServiceStatusCallback>& callback,
    int32_t* _aidl_return) {
  LOG(DEBUG) << __FUNCTION__ << ": Service " << service;

  std::lock_guard<std::mutex> lock(mManagerLock);

  auto iter = mServiceInfoMap.find(service);
  if (iter != mServiceInfoMap.end()) {
    *_aidl_return = static_cast<int32_t>(SERVICE_ALREADY_STARTED);
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
  LOG(DEBUG) << __FUNCTION__ << ": Service " << service;

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
  mManagerControl = std::make_unique<ManagerControlClient>();
  mManagerControl->registerErrorHandler([&]() {
    notifyAllServicesFatal();
  });
  return mManagerControl->powerOn();
}

void ManagerServer::powerOff() {
  if (mComm) { mComm->close(); }
  if (mManagerControl) { mManagerControl->powerOff(); }
}

binder::Status ManagerServer::suspend(int32_t service, int32_t* _aidl_return) {
  std::lock_guard<std::mutex> lock(mManagerLock);
  if (!isServiceStarted(service)) {
    *_aidl_return = static_cast<int32_t>(SERVICE_NOT_STARTED);
    return binder::Status::ok();
  }

  mServiceInfoMap[service].stateRequest = SUSPEND;

  if (areAllServicesSuspend()) {
    int res = mManagerControl->suspend();
    if (res != 0) {
      LOG(ERROR) << __FUNCTION__ << ": Failed to suspend Easel:"
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

  std::lock_guard<std::mutex> lock(mManagerLock);
  if (!isServiceStarted(service)) {
    *_aidl_return = static_cast<int32_t>(SERVICE_NOT_STARTED);
    return binder::Status::ok();
  }

  int retCode = SUCCESS;
  if (mEaselResumed) {
    LOG(DEBUG) << __FUNCTION__ << ": Easel is already resumed.";
    mServiceInfoMap[service].stateRequest = RESUME;
  } else {
    int res = mManagerControl->resume();
    if (res != 0) {
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
