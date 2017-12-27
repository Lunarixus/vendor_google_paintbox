#define LOG_TAG "EaselManagerClientExample"

#include "EaselManager.h"

#include "android-base/logging.h"
#include "android/EaselManager/BnServiceStatusCallback.h"

// An example to use EaselManager start and stop a service.

namespace android {
namespace EaselManager {

// A mock app service status callback that logs the status change.
class ServiceStatusCallback : public BnServiceStatusCallback {
 public:
  ServiceStatusCallback(Service service) : mService(service),
                                           mServiceStart(false),
                                           mServiceStop(false) {}

  binder::Status onServiceStart() override {
    LOG(INFO) << __FUNCTION__ << ": Service " << mService << " started";
    std::unique_lock<std::mutex> startLock(mServiceStartLock);
    mServiceStart = true;
    mServiceStartCond.notify_one();
    return binder::Status::ok();
  }

  binder::Status onServiceEnd(int exit) override {
    LOG(INFO) << __FUNCTION__ << ": Service " << mService
                              << " stopped, exit " << exit;
    std::unique_lock<std::mutex> stopLock(mServiceStopLock);
    mServiceStop = true;
    mServiceStopCond.notify_one();
    return binder::Status::ok();
  }

  binder::Status onServiceError(int32_t error) override {
    LOG(INFO) << __FUNCTION__ << ": Service " << mService << " error " << error;
    return binder::Status::ok();
  }

  void wait() {
    {
      std::unique_lock<std::mutex> startLock(mServiceStartLock);
      mServiceStartCond.wait(startLock, [&] { return mServiceStart; });
    }
    {
      std::unique_lock<std::mutex> stopLock(mServiceStopLock);
      mServiceStopCond.wait(stopLock, [&] { return mServiceStop; });
    }
  }

 private:
  Service mService;
  bool mServiceStart;
  bool mServiceStop;
  std::mutex mServiceStartLock;
  std::mutex mServiceStopLock;
  std::condition_variable mServiceStartCond;
  std::condition_variable mServiceStopCond;
};

}  // namespace EaselManager
}  // namespace android

using android::EaselManager::ServiceStatusCallback;
using android::EaselManager::ManagerClient;
using android::sp;

int main() {
  std::unique_ptr<ManagerClient> client = ManagerClient::create();
  CHECK(client->initialize() == android::EaselManager::SUCCESS);

  auto dummy_service = android::EaselManager::DUMMY_SERVICE_1;
  sp<ServiceStatusCallback> dummy_callback(
      new ServiceStatusCallback(dummy_service));
  CHECK(client->startService(dummy_service, dummy_callback) ==
        android::EaselManager::SUCCESS);
  CHECK(client->stopService(dummy_service) == android::EaselManager::SUCCESS);
  // Wait for app service to be stopped
  dummy_callback->wait();

  auto crash_service = android::EaselManager::CRASH_SERVICE;
  sp<ServiceStatusCallback> crash_callback(
      new ServiceStatusCallback(crash_service));
  CHECK(client->startService(crash_service, crash_callback) ==
        android::EaselManager::SUCCESS);
  // Wait for app service crash.
  crash_callback->wait();
}
