#define LOG_TAG "EaselManagerClientExample"

#include "EaselManager.h"

#include "android-base/logging.h"
#include "android/EaselManager/BnAppStatusCallback.h"

// An example to use EaselManager start and stop an app.

namespace android {
namespace EaselManager {

// A mock app status callback that logs the status change.
class AppStatusCallback : public BnAppStatusCallback {
 public:
  AppStatusCallback(App app) : mApp(app), mAppStart(false), mAppStop(false) {}

  binder::Status onAppStart() override {
    LOG(INFO) << __FUNCTION__ << ": App " << mApp << " started";
    std::unique_lock<std::mutex> startLock(mAppStartLock);
    mAppStart = true;
    mAppStartCond.notify_one();
    return binder::Status::ok();
  }

  binder::Status onAppEnd() override {
    LOG(INFO) << __FUNCTION__ << ": App " << mApp << " stopped";
    std::unique_lock<std::mutex> stopLock(mAppStopLock);
    mAppStop = true;
    mAppStopCond.notify_one();
    return binder::Status::ok();
  }

  binder::Status onAppError(int32_t error) override {
    LOG(INFO) << __FUNCTION__ << ": App " << mApp << " error " << error;
    return binder::Status::ok();
  }

  void wait() {
    {
      std::unique_lock<std::mutex> startLock(mAppStartLock);
      mAppStartCond.wait(startLock, [&] { return mAppStart; });
    }
    {
      std::unique_lock<std::mutex> stopLock(mAppStopLock);
      mAppStopCond.wait(stopLock, [&] { return mAppStop; });
    }
  }

 private:
  App mApp;
  bool mAppStart;
  bool mAppStop;
  std::mutex mAppStartLock;
  std::mutex mAppStopLock;
  std::condition_variable mAppStartCond;
  std::condition_variable mAppStopCond;
};

}  // namespace EaselManager
}  // namespace android

using android::EaselManager::AppStatusCallback;
using android::EaselManager::ManagerClient;
using android::sp;

int main() {
  std::unique_ptr<ManagerClient> client = ManagerClient::create();
  CHECK(client->initialize() == android::EaselManager::SUCCESS);

  auto dummy_app = android::EaselManager::DUMMY_APP;
  sp<AppStatusCallback> dummy_callback(new AppStatusCallback(dummy_app));
  CHECK(client->startApp(dummy_app, dummy_callback) ==
        android::EaselManager::SUCCESS);
  CHECK(client->stopApp(dummy_app) == android::EaselManager::SUCCESS);
  // Wait for app to be stopped
  dummy_callback->wait();

  auto crash_app = android::EaselManager::CRASH_APP;
  sp<AppStatusCallback> crash_callback(new AppStatusCallback(crash_app));
  CHECK(client->startApp(crash_app, crash_callback) ==
        android::EaselManager::SUCCESS);
  // Wait for app crash.
  crash_callback->wait();
}
