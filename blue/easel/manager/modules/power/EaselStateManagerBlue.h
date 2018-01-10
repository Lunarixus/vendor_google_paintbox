#ifndef __EASEL_STATE_MANAGER_H__
#define __EASEL_STATE_MANAGER_H__

#include <stdio.h>
#include <unistd.h>

#include "uapi/linux/mnh-sm.h"

namespace android {
namespace EaselPowerBlue {

class EaselStateManager {
 public:
  enum State {
    ESM_STATE_OFF = MNH_STATE_OFF,  // powered off
    ESM_STATE_ACTIVE = MNH_STATE_ACTIVE,  // powered on and booted
    ESM_STATE_SUSPEND = MNH_STATE_SUSPEND,  // suspended, ddr in self-refresh
    ESM_STATE_MAX = MNH_STATE_MAX,
  };

  EaselStateManager(): mFd(-1) {}

  int open();
  int close();

  /*
   * Gets the current system state.
   *
   * state: reference to current state, set in method.
   *
   * Returns 0 for success; otherwise, returns error number.
   */
  int getState(enum State *state);

  /*
   * Sets the current system state.
   *
   * state: desired state.
   * blocking: use "true" to wait until state transition has occurred; use
   *           "false" if method should return immediately.
   *
   * Returns 0 for success; otherwise, returns error number.
   */
  int setState(enum State state, bool blocking = true);

  /*
   * Blocks until Easel is powered, so PCIe transactions can occur.
   *
   * Returns 0 for success; otherwise, returns error number.
   */
  int waitForPower();

  /*
   * Blocks until state is reached.
   *
   * state: desired state.
   *
   * Returns 0 for success; otherwise, returns error number.
   */
  int waitForState(enum State state);

  /*
   * Retrieves the firmware version.
   *
   * fwVersion[out]: version string, set in method.  Not NUL-terminated.
   * size[in]:       buffer size (in bytes) of fwVersion
   *
   * Returns 0 for success; otherwise, returns error number.
   */
  int getFwVersion(char *fwVersion, size_t size);

 private:
  int mFd;
};

} // namespace EaselPowerBlue
} // namespace android

#endif  // __EASEL_STATE_MANAGER_H__
