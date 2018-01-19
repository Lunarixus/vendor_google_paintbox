#include "EaselPowerBlue.h"

int main() {
  android::EaselPowerBlue::EaselPowerServerBlue server;
  fprintf(stdout, "easel_power_server\n");
  fflush(stdout);
  server.run();
  // Code should not reach here.
  return 1;
}
