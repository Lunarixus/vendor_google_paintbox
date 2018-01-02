#define LOG_TAG "ManagerControlClient"

#include <utils/Errors.h>
#include "android-base/logging.h"

#include "ManagerUtils.h"
#include "ManagerControlClient.h"

namespace android {
namespace EaselManager {

ManagerControlClient::ManagerControlClient() {
  initialize();
}

ManagerControlClient::~ManagerControlClient() {
  powerOff();
}

void ManagerControlClient::registerErrorHandler(ErrorHandler handler) {
  std::lock_guard<std::mutex> l(mEaselControlLock);
  mErrorHandler = handler;
}

void ManagerControlClient::initialize() {
  std::lock_guard<std::mutex> l(mEaselControlLock);
  int res = retryFunction([&]() {
    return mEaselControl.open(EASEL_SERVICE_MANAGER_SYSCTRL);
  });
  if (!res) {
    LOG(ERROR) << __FUNCTION__ << ": Failed to open Easel control: "
               << strerror(errno) << " (" << -errno << ").";
    return;
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

    mErrorHandler();

    return 0;
  });
}

int ManagerControlClient::powerOn() {
  LOG(DEBUG) << __FUNCTION__ << "Easel power on";

  std::lock_guard<std::mutex> l(mEaselControlLock);
  if (!mEaselControlOpened) {
    LOG(ERROR) << __FUNCTION__ << ": Easel control is not opened.";
    return NO_INIT;
  }
  // TODO(b/70727332): remove "resume" call when libeaselcontrol has "powerOn".
  int res = retryFunction([&]() { return mEaselControl.resume(); });
  if (!res) {
    LOG(ERROR) << __FUNCTION__ << ": Resume Easel failed: " << strerror(-res);
    return res;
  }
  // TODO(b/70727332): "resume" is non-blocking, so sleep 1 second to make sure
  // Easel is resumed. This will go away when libeaselcontrol "blue" is
  // implemented, as it will be blocking.
  sleep(1);

  return res;
}

void ManagerControlClient::powerOff() {
  LOG(DEBUG) << __FUNCTION__ << "Easel power off";

  std::lock_guard<std::mutex> l(mEaselControlLock);
  if (mEaselControlOpened) {
    mEaselControl.close();
    mEaselControlOpened = false;
  }
}

int ManagerControlClient::suspend() {
  LOG(DEBUG) << __FUNCTION__ << ": Suspending Easel.";

  std::lock_guard<std::mutex> l(mEaselControlLock);
  if (!mEaselControlOpened) {
    LOG(ERROR) << __FUNCTION__ << ": Easel control is not opened.";
    return NO_INIT;
  }
  if (!mEaselResumed) { return 0; }
  int res = mEaselControl.suspend();
  if (!res) {
    LOG(ERROR) << __FUNCTION__ << ": Suspend Easel failed: " << strerror(-res);
  } else {
    mEaselResumed = false;
  }
  return res;
}

int ManagerControlClient::resume() {
  LOG(DEBUG) << __FUNCTION__ << ": Resuming Easel.";

  std::lock_guard<std::mutex> l(mEaselControlLock);
  if (!mEaselControlOpened) {
    LOG(ERROR) << __FUNCTION__ << ": Easel control is not opened.";
    return NO_INIT;
  }

  if (mEaselResumed) {
    LOG(DEBUG) << __FUNCTION__ << ": Easel is already resumed.";
    return 0;
  }

  int res = mEaselControl.resume();
  if (!res) {
    LOG(ERROR) << __FUNCTION__ << ": Resume Easel failed: " << strerror(-res);
  } else {
    mEaselResumed = true;
  }
  return res;
}

}  // namespace EaselManager
}  // namespace android
