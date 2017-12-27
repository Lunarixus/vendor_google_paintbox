#define STR(s) #s
#define STR2(s) STR(s)
#define LOG_TAG "easeldummyapp" STR2(DUMMY_APP)

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

  LOG(INFO) << "Dummy app " << DUMMY_APP << " starting...";
  pause();
  exit(0);
}
