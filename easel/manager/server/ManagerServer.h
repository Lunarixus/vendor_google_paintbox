#ifndef PAINTBOX_MANAGER_SERVER_H
#define PAINTBOX_MANAGER_SERVER_H

#include "EaselComm2.h"
#include "ManagerService.h"

namespace EaselManagerService {

// Easel side server that listens app management requests from Android AP
// client.
class ManagerServer {
 public:
  ManagerServer();
  // Starts the server. This function will not return.
  void run();

 private:
  std::unique_ptr<EaselComm2::Comm> mComm;
  std::unique_ptr<ManagerService> mService;
};

}  // namespace EaselManagerService

#endif  // PAINTBOX_MANAGER_SERVER_H
