#define LOG_TAG "easelcrashapp"

#include "android-base/logging.h"

int main() {
  sleep(1);
  LOG(FATAL) << "App crashing...";
}