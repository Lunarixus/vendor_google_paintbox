#define LOG_TAG "easeldummyapp"

#include <signal.h>

#include "android-base/logging.h"

void termHandler(int signal) {
  LOG(INFO) << "Received signal " << signal << ", exiting...";
}

int main() {
  struct sigaction action;
  memset(&action, 0, sizeof(struct sigaction));
  action.sa_handler = termHandler;
  sigaction(SIGTERM, &action, nullptr);

  LOG(INFO) << "Dummy app starting...";
  pause();
  exit(0);
}