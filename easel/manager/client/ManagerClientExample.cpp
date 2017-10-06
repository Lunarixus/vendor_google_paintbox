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
      mAppStartCond.wait(startLock, [&] {return mAppStart;});
    }
    {
      std::unique_lock<std::mutex> stopLock(mAppStopLock);
      mAppStopCond.wait(stopLock, [&] {return mAppStop;});
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

int main() {
  std::unique_ptr<android::EaselManager::ManagerClient> client =
      android::EaselManager::ManagerClient::create();
  auto app = android::EaselManager::PBSERVER;
  android::EaselManager::AppStatusCallback callback(app);

  CHECK(client->initialize() == android::EaselManager::SUCCESS);
  CHECK(client->startApp(app, &callback) == android::EaselManager::SUCCESS);
  CHECK(client->stopApp(app) == android::EaselManager::SUCCESS);

  callback.wait();
}
