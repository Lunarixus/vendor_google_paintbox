#ifndef EASEL_POWER_BLUE_H
#define EASEL_POWER_BLUE_H

#include <memory>
#include <mutex>
#include <string>

#include "hardware/gchips/paintbox/system/include/easel_comm.h"

namespace android {
namespace EaselPowerBlue {

class EaselStateManager;

enum Channel {
  HANDSHAKE_CHANNEL = 1,
  SUSPEND_CHANNEL = 2,
};

/*
 * EaselPowerServerBlue
 *
 * EaselPowerServerBlue class starts a server handling incoming power operation
 * requests, such as suspend-to-ram.  It is recommended that server-side daemon
 * process run EaselPowerServerBlue at the beginning.
 *
 * EaselPowerServerBlue occupies EASEL_SERVICE_SYSCTRL (service id = 0) for
 * communication with client.
 */
class EaselPowerServerBlue {
 public:
  EaselPowerServerBlue();
  EaselPowerServerBlue(const EaselPowerServerBlue&) = delete;
  EaselPowerServerBlue& operator=(const EaselPowerServerBlue&) = delete;
  ~EaselPowerServerBlue();

  void run();

 private:
  void suspendHandler(const easel::Message& message);
  std::unique_ptr<easel::Comm> mComm;

  std::mutex mSuspendLock;

  /*
   * Counter for suspend request received.
   */
  int suspend_count_;  // GUARDED_BY(mSuspendLock)
};

/*
 * EaselPowerBlue
 *
 * EaselPowerBlue class is the power module of Easel Manager, controlling
 * power states of Easel from client side.
 *
 * EaselPowerBlue occupies EASEL_SERVICE_SYSCTRL (service id = 0) for
 * communication with server.
 */
class EaselPowerBlue {
 public:
  EaselPowerBlue();
  EaselPowerBlue(const EaselPowerBlue&) = delete;
  EaselPowerBlue& operator=(const EaselPowerBlue&) = delete;
  ~EaselPowerBlue();

  /*
   * Opens Easel Power Manager module.
   * Required to be called before any actual power operation.
   *
   * Returns 0 on success or error code on failure.
   */
  int open();

  /*
   * Closes Easel Power Manager module.
   * Required to be called before deleting Easel Power.  All power operations
   * become invalid after this call, until open() is called again.
   */
  void close();

  /*
   * Powers on Easel blockingly.
   *
   * Returns 0 on success or error code on failure.
   */
  int powerOn();

  /*
   * Powers off Easel blockingly.
   *
   * Returns 0 on success or error code on failure.
   */
  int powerOff();

  /*
   * Resumes Easel blockingly.
   *
   * Returns 0 on success or error code on failure.
   */
  int resume();

  /*
   * Suspends Easel blockingly.
   *
   * Returns 0 on success or error code on failure.
   */
  int suspend();

  /*
   * Retrieves Easel firmware version.
   * Requires open() to be called before calling this method.
   *
   * Returns a string literal of fw version on success;
   *         error code on failure.
   */
  std::string getFwVersion();

 private:
  /*
   * State manager instance.
   */
  std::unique_ptr<EaselStateManager> mStateManager;
  std::unique_ptr<easel::Comm> mComm;
};

} // namespace EaselPowerBlue
} // namespace android

#endif  // EASEL_POWER_BLUE_H
