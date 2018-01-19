#include <thread>

#include "EaselPowerBlue.h"
#include "ManagerServer.h"

int main() {
  // Starts a power module server
  android::EaselPowerBlue::EaselPowerServerBlue powerServer;
  std::thread([&](){ powerServer.run(); }).detach();

  // Starts a manager server
  EaselManagerService::ManagerServer server;
  server.run();
  // Code should not reach here.
  return 1;
}