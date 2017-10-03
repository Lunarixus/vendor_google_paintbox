#include "ManagerServer.h"

int main() {
  android::EaselManager::ManagerServer::publishAndJoinThreadPool();
  // Code should never reach here.
  return 1;
}
