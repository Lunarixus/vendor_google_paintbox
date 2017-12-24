#include "ManagerServer.h"

int main() {
  android::ProcessState::initWithDriver("/dev/vndbinder");
  android::EaselManager::ManagerServer::publishAndJoinThreadPool();
  // Code should never reach here.
  return 1;
}
