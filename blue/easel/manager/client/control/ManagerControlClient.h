#ifndef PAINTBOX_MANAGER_CONTROL_CLIENT_H
#define PAINTBOX_MANAGER_CONTROL_CLIENT_H

#include <mutex>

#include "easelcontrol.h"

namespace android {
namespace EaselManager {

class ManagerControlClient {
 public:
  using ErrorHandler = std::function<void()>;

  ManagerControlClient();
  ~ManagerControlClient();
  ManagerControlClient(const ManagerControlClient&) = delete;
  ManagerControlClient& operator=(const ManagerControlClient&) = delete;

  /*
   * Powers on Easel
   *
   * Returns zero for success or error code for failure.
   */
  int powerOn();

  /*
   * Powers off Easel
   */
  void powerOff();

  /*
   * Suspends Easel
   *
   * Returns zero for success or error code for failure.
   */
  int suspend();

  /*
   * Resumes Easel. If Easel is suspended, then this will resume it.
   *
   * Returns zero for success or error code for failure.
   */
  int resume();

  /*
   * Registers a callback on error.
   */
  void registerErrorHandler(ErrorHandler handler);

 private:
  void initialize();

  ErrorHandler mErrorHandler;
  std::mutex mEaselControlLock;
  // Easel control client. Protected by mEaselControlLock.
  EaselControlClient mEaselControl;
  // If Easel control client is opened. Protected by mEaselControlLock.
  bool mEaselControlOpened;
  // If Easel is resumed. Protected by mEaselControlLock.
  bool mEaselResumed;
};

}  // namespace EaselManager
}  // namespace android

#endif  // PAINTBOX_MANAGER_CONTROL_CLIENT_H
