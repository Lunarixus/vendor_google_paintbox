#ifndef PAINTBOX_MANAGER_SERVER_H
#define PAINTBOX_MANAGER_SERVER_H

#include "hardware/gchips/paintbox/system/include/easel_comm.h"
#include "hardware/gchips/paintbox/system/include/easel_comm_helper.h"

#include "ManagerService.h"

namespace EaselManagerService {

// Easel side server that listens app service management requests from Android
// AP client.
class ManagerServer {
 public:
  ManagerServer();
  // Starts the server. This function will not return.
  void run();

 private:
  std::unique_ptr<easel::Comm> mComm;
  std::unique_ptr<ManagerService> mService;

  std::unique_ptr<easel::FunctionHandler> mStartServiceStatusHandler;
  std::unique_ptr<easel::FunctionHandler> mStopServiceStatusHandler;
};

}  // namespace EaselManagerService

#endif  // PAINTBOX_MANAGER_SERVER_H
