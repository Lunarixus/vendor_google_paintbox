#define LOG_TAG "EaselPowerBlue"

#include "EaselPowerBlue.h"

#include <string>

#include "android-base/logging.h"
#include "hardware/gchips/paintbox/system/include/easel_comm.h"
#include "hardware/gchips/paintbox/system/include/easel_comm_helper.h"
#include "ScopedTimeLogger.h"

namespace android {
namespace EaselPowerBlue {

int EaselPowerBlue::open() {
  int ret = mStateManager.open();
  if (ret) {
    LOG(ERROR) << "failed to initialize state manager: " << strerror(-ret);
    return ret;
  }

  mComm = easel::CreateComm(easel::Comm::Type::CLIENT);
  if (!mComm) {
    LOG(ERROR) << "failed to create comm object";
    ret = mStateManager.close();
    if (ret) {
      LOG(WARNING) << "error when closing state manager: " << strerror(-ret);
    }
    return -ENODEV;
  }

  LOG(INFO) << "did open";
  return 0;
}

void EaselPowerBlue::close() {
  mComm->Close(); // closes down communication
  mComm.reset();  // releases easelcomm object

  int ret = mStateManager.close();
  if (ret) {
    LOG(WARNING) << "error when closing state manager: " << strerror(-ret);
    return;
  }

  LOG(INFO) << "did close";
}

int EaselPowerBlue::powerOn() {
  MEASURE_SCOPED_TIME(__FUNCTION__);

  int ret = mStateManager.setState(EaselStateManager::ESM_STATE_ACTIVE, /*blocking=*/true);
  if (ret) {
    LOG(ERROR) << "failed to power on: " << strerror(-ret);
    return ret;
  }

  ret = mComm->Open(easel::EASEL_SERVICE_SYSCTRL, /*timeout_ms=*/500);
  if (ret) {
    LOG(ERROR) << "failed to open sysctrl comm channel: " << strerror(-ret);
    return ret;
  }

  return 0;
}

int EaselPowerBlue::powerOff() {
  MEASURE_SCOPED_TIME(__FUNCTION__);

  mComm->Close(); // closes down communication

  int ret = mStateManager.setState(EaselStateManager::ESM_STATE_OFF, /*blocking=*/true);
  if (ret) {
    LOG(ERROR) << "failed to power off: " << strerror(-ret);
    return ret;
  }

  return 0;
}

int EaselPowerBlue::resume() {
  MEASURE_SCOPED_TIME(__FUNCTION__);

  int ret = mStateManager.setState(EaselStateManager::ESM_STATE_ACTIVE, /*blocking=*/true);
  if (ret) {
    LOG(ERROR) << "failed to resume: " << strerror(-ret);
    return ret;
  }

  return 0;
}

int EaselPowerBlue::suspend() {
  MEASURE_SCOPED_TIME(__FUNCTION__);

  mComm->Send(SUSPEND_CHANNEL, /*payload=*/nullptr);

  int ret = mStateManager.setState(EaselStateManager::ESM_STATE_SUSPEND, /*blocking=*/true);
  if (ret) {
    LOG(ERROR) << "failed to suspend: " << strerror(-ret);
    return ret;
  }

  return 0;
}

std::string EaselPowerBlue::getFwVersion() {
  // Compile time check that FW_VER_SIZE should be at least 1 character
  static_assert(FW_VER_SIZE > 0,
                "fw version string too short; please check kernel header");

  char fwVersion[FW_VER_SIZE];
  int ret = mStateManager.getFwVersion(fwVersion, sizeof(fwVersion));
  if (ret) {
    LOG(ERROR) << "failed to get fw version: " << strerror(-ret);
    return "N/A";
  }

  std::string retString(fwVersion, FW_VER_SIZE);
  LOG(INFO) << "did get fw version [" << retString << "]";
  return retString;
}

} // namespace EaselPowerBlue
} // namespace android
