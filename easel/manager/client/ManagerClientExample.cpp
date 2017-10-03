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
  AppStatusCallback(App app) : mApp(app) {}

  binder::Status onAppStart() override {
    LOG(INFO) << __FUNCTION__ << ": App " << mApp << " started";
    return binder::Status::ok();
  }

  binder::Status onAppEnd() override {
    LOG(INFO) << __FUNCTION__ << ": App " << mApp << " stopped";
    return binder::Status::ok();
  }

 private:
  App mApp;
};

}  // namespace EaselManager
}  // namespace android

int main() {
  std::unique_ptr<android::EaselManager::ManagerClient> client =
      android::EaselManager::ManagerClient::create();
  auto app = android::EaselManager::PBSERVER;
  CHECK(client->initialize() == android::EaselManager::SUCCESS);
  CHECK(client->startApp(app, new android::EaselManager::AppStatusCallback(
                                  app)) == android::EaselManager::SUCCESS);
  CHECK(client->stopApp(app) == android::EaselManager::SUCCESS);
}
