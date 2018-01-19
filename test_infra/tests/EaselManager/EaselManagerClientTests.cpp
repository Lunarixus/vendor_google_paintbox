#define LOG_TAG "EaselManagerClientTests"

#include "EaselManager.h"

#include "android-base/logging.h"
#include "android/EaselManager/BnServiceStatusCallback.h"

#include "gtest/gtest.h"

static const int kWaitTime = 3;  // seconds

namespace android {
namespace EaselManager {

class ServiceStatusCallback : public BnServiceStatusCallback {
 public:
  ServiceStatusCallback(Service service, int exit) : mService(service),
                                                     mExitCode(exit),
                                                     mServiceStart(false),
                                                     mServiceStop(false) {
  }

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
    EXPECT_EQ(exit, mExitCode);
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
  int mExitCode;
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

TEST_F(EaselManagerClientTests, TestStartOneServiceTwice) {
  auto dummy_service = android::EaselManager::DUMMY_SERVICE_1;
  // when app is mocked to exit on easel, "exit" is expected to be "SIGTERM",
  // which matches the exit code set in "dummy_app.cpp".
  sp<ServiceStatusCallback> dummy_callback(
      new ServiceStatusCallback(dummy_service, SIGTERM));
  ASSERT_EQ(client->startService(dummy_service, dummy_callback),
            android::EaselManager::SUCCESS);
  ASSERT_EQ(client->startService(dummy_service, dummy_callback),
            android::EaselManager::SERVICE_ALREADY_STARTED);
  ASSERT_EQ(client->stopService(dummy_service), android::EaselManager::SUCCESS);
  // Wait for app service to be stopped
  dummy_callback->wait();
}

TEST_F(EaselManagerClientTests, TestStopOneServiceTwice) {
  auto dummy_service = android::EaselManager::DUMMY_SERVICE_1;
  sp<ServiceStatusCallback> dummy_callback(
      new ServiceStatusCallback(dummy_service, SIGTERM));
  ASSERT_EQ(client->startService(dummy_service, dummy_callback),
            android::EaselManager::SUCCESS);
  ASSERT_EQ(client->stopService(dummy_service), android::EaselManager::SUCCESS);
  // Sleep some seconds to wait easel server send back stop response.
  sleep(kWaitTime);
  ASSERT_EQ(client->stopService(dummy_service),
            android::EaselManager::SERVICE_NOT_STARTED);
  // Wait for app service to be stopped
  dummy_callback->wait();
}

TEST_F(EaselManagerClientTests, TestDummyService) {
  auto dummy_service = android::EaselManager::DUMMY_SERVICE_1;
  sp<ServiceStatusCallback> dummy_callback(
      new ServiceStatusCallback(dummy_service, SIGTERM));
  ASSERT_EQ(client->startService(dummy_service, dummy_callback),
            android::EaselManager::SUCCESS);
  ASSERT_EQ(client->stopService(dummy_service), android::EaselManager::SUCCESS);
  // Wait for app service to be stopped
  dummy_callback->wait();
}

TEST_F(EaselManagerClientTests, TestCrashService) {
  auto crash_service = android::EaselManager::CRASH_SERVICE;
  // when app service crashed on easel, "exit" is expected to be "SIGABRT"
  sp<ServiceStatusCallback> crash_callback(
      new ServiceStatusCallback(crash_service, SIGABRT));
  ASSERT_EQ(client->startService(crash_service, crash_callback),
            android::EaselManager::SUCCESS);
  // Wait for app service crash.
  crash_callback->wait();
}

TEST_F(EaselManagerClientTests, TestStartMultiServices) {
  auto dummy_service_1 = android::EaselManager::DUMMY_SERVICE_1;
  sp<ServiceStatusCallback> dummy_callback_1(
  new ServiceStatusCallback(dummy_service_1, SIGTERM));
  auto dummy_service_2 = android::EaselManager::DUMMY_SERVICE_2;
  sp<ServiceStatusCallback> dummy_callback_2(
  new ServiceStatusCallback(dummy_service_2, SIGTERM));

  ASSERT_EQ(client->startService(dummy_service_1, dummy_callback_1),
            android::EaselManager::SUCCESS);
  ASSERT_EQ(client->startService(dummy_service_2, dummy_callback_2),
            android::EaselManager::SUCCESS);
  ASSERT_EQ(client->stopService(dummy_service_1),
            android::EaselManager::SUCCESS);
  ASSERT_EQ(client->stopService(dummy_service_2),
            android::EaselManager::SUCCESS);
  // Wait for app service to be stopped
  dummy_callback_1->wait();
  dummy_callback_2->wait();
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
