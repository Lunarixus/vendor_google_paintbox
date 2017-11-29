#define LOG_TAG "EaselManagerClientTests"

#include "EaselManager.h"

#include "android-base/logging.h"
#include "android/EaselManager/BnAppStatusCallback.h"

#include "gtest/gtest.h"

namespace android {
namespace EaselManager {

class AppStatusCallback : public BnAppStatusCallback {
 public:
  AppStatusCallback(App app, int exit) : mApp(app),
                                         mExitCode(exit),
                                         mAppStart(false),
                                         mAppStop(false) {
  }

  binder::Status onAppStart() override {
    LOG(INFO) << __FUNCTION__ << ": App " << mApp << " started";
    std::unique_lock<std::mutex> startLock(mAppStartLock);
    mAppStart = true;
    mAppStartCond.notify_one();
    return binder::Status::ok();
  }

  binder::Status onAppEnd(int exit) override {
    LOG(INFO) << __FUNCTION__ << ": App " << mApp << " stopped, exit " << exit;
    EXPECT_EQ(exit, mExitCode);
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
  int mExitCode;
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

class EaselManagerClientTests : public ::testing::Test {
 public:
  EaselManagerClientTests() {
    client = ManagerClient::create();
  }

 protected:
  void SetUp() override {
    ASSERT_EQ(client->initialize(), android::EaselManager::SUCCESS);
  }

 public:
  std::unique_ptr<ManagerClient> client;
};

TEST_F(EaselManagerClientTests, TestDummyApp) {
  auto dummy_app = android::EaselManager::DUMMY_APP;
  // when app is mocked to exit on easel, "exit" is expected to be "SIGTERM",
  // which matches the exit code set in "dummy_app.cpp".
  sp<AppStatusCallback> dummy_callback(
      new AppStatusCallback(dummy_app, SIGTERM));
  ASSERT_EQ(client->startApp(dummy_app, dummy_callback),
            android::EaselManager::SUCCESS);
  ASSERT_EQ(client->stopApp(dummy_app), android::EaselManager::SUCCESS);
  // Wait for app to be stopped
  dummy_callback->wait();
}

TEST_F(EaselManagerClientTests, TestCrashApp) {
  auto crash_app = android::EaselManager::CRASH_APP;
  // when app crashed on easel, "exit" is expected to be "SIGABRT"
  sp<AppStatusCallback> crash_callback(
      new AppStatusCallback(crash_app, SIGABRT));
  ASSERT_EQ(client->startApp(crash_app, crash_callback),
            android::EaselManager::SUCCESS);
  // Wait for app crash.
  crash_callback->wait();
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
