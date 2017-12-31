#define LOG_TAG "EaselManagerServer"

#include "ManagerServer.h"

#include <fstream>

#include "ManagerShared.h"

#include "android-base/logging.h"
#include "binder/IServiceManager.h"

#include "EaselManager.h"
#include "EaselManagerCommon.h"
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
    default:
      LOG(FATAL) << "Error " << error << " not defined";
  }
}

const char* kPowerOn = "/sys/devices/virtual/misc/mnh_sm/download";
const char* kPowerOff = "/sys/devices/virtual/misc/mnh_sm/poweroff";
const char* kStageFw = "/sys/devices/virtual/misc/mnh_sm/stage_fw";
const char* kSysState = "/sys/devices/virtual/misc/mnh_sm/state";

enum State {
  POWER_ON = 1,
  POWER_OFF = 0,
};

// Reads a sysfs node.
void readSysfsNode(const char* node) {
  std::string s;
  std::ifstream f(node);
  f >> s;
}

// Writes a int to sysfs node.
void writeSysfsNode(const char* node, int value) {
  std::ofstream f(node);
  f << value;
}

// Compares a sysfs node value with target and returns true if matched.
bool matchSysfsNode(const char* node, int target) {
  int value = 0;
  std::ifstream f(node);
  f >> value;
  return value == target;
}

}  // namespace

ManagerServer::ManagerServer() {
  mComm = EaselComm2::Comm::create(EaselComm2::Comm::Mode::CLIENT);
  initialize();
}

ManagerServer::~ManagerServer() { powerOff(); }

char const* ManagerServer::getServiceName() { return gEaselManagerService; }

void ManagerServer::initialize() {
  mComm->registerHandler(SERVICE_STATUS,
                         [&](const EaselComm2::Message& message) {
    EaselManagerService::ServiceStatusResponse response;
    if (!message.toProto(&response)) {
      LOG(ERROR) << "Could not parse response.";
      return;
    }

    std::lock_guard<std::mutex> lock(mManagerLock);

    auto iter = mServiceCallbackMap.find(convertService(response.service()));
    if (iter == mServiceCallbackMap.end()) {
      LOG(ERROR) << "Could not find service " << response.service();
      return;
    }

    if (response.error() != EaselManagerService::SUCCESS) {
      iter->second->onServiceError(convertError(response.error()));
      // Immediately clears the callback if happen occurs.
      // Client will not get any new updates about this service
      // Until new callback registers with startService call.
      mServiceCallbackMap.erase(iter);
    } else {
      if (response.status() == EaselManagerService::LIVE) {
        LOG(INFO) << "Service " << response.service() << " started";
        iter->second->onServiceStart();
      } else if (response.status() == EaselManagerService::EXIT) {
        LOG(INFO) << "Service " << response.service()
                  << " stopped, exit " <<  response.exit();
        iter->second->onServiceEnd(response.exit());
        mServiceCallbackMap.erase(iter);
      } else {
        LOG(FATAL) << "Service " << response.service()
                   << " unknown but no error reported";
      }
    }

    if (mServiceCallbackMap.empty()) {
      powerOff();
    }
  });
}

binder::Status ManagerServer::startService(
    int32_t service,
    const sp<IServiceStatusCallback>& callback,
    int32_t* _aidl_return) {
  LOG(INFO) << __FUNCTION__ << ": Service " << service;

  std::lock_guard<std::mutex> lock(mManagerLock);

  auto iter = mServiceCallbackMap.find(service);
  if (iter != mServiceCallbackMap.end()) {
    *_aidl_return = static_cast<int32_t>(SERVICE_ALREADY_STARTED);
    return binder::Status::ok();
  }

  mServiceCallbackMap[service] = callback;

  EaselManagerService::StartServiceRequest request;
  request.set_service(convertService(service));

  if (!mComm->connected()) {
    int res = powerOn();
    if (res != 0) {
      *_aidl_return = static_cast<int32_t>(EASEL_POWER_ERROR);
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

  auto iter = mServiceCallbackMap.find(service);
  if (iter != mServiceCallbackMap.end()) {
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
  if (!matchSysfsNode(kSysState, POWER_ON)) {
    writeSysfsNode(kStageFw, 1);
    readSysfsNode(kPowerOn);
  }

  // Open the channel.
  int res = mComm->open(EASEL_SERVICE_MANAGER, /*timeout_ms=mm*/100);
  if (res != 0) return res;
  return mComm->startReceiving();
}

void ManagerServer::powerOff() {
  mComm->close();
  LOG(INFO) << "Easel power off";
  if (!matchSysfsNode(kSysState, POWER_OFF)) {
    readSysfsNode(kPowerOff);
  }
}

}  // namespace EaselManager
}  // namespace android
