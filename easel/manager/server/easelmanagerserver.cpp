#include "ManagerServer.h"

int main() {
  EaselManagerService::ManagerServer server;
  server.run();
  // Code should not reach here.
  return 1;
}