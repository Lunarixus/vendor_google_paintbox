#define LOG_TAG "EaselPowerServerBlue"

#include "EaselPowerBlue.h"

#include <fcntl.h>
#include <mutex>

#include "android-base/logging.h"
#include "hardware/gchips/paintbox/system/include/easel_comm.h"
#include "hardware/gchips/paintbox/system/include/easel_comm_helper.h"

#define KERNEL_SUSPEND_SYS_FILE    "/sys/power/state"
#define KERNEL_SUSPEND_STRING      "mem"

namespace android {
namespace EaselPowerBlue {

EaselPowerServerBlue::EaselPowerServerBlue() {
  suspend_count_ = 0;
}

EaselPowerServerBlue::~EaselPowerServerBlue() {}

void EaselPowerServerBlue::run() {
  mComm = easel::CreateComm(easel::Comm::Type::SERVER);
  mComm->RegisterHandler(SUSPEND_CHANNEL, std::make_unique<easel::FunctionHandler>(
      [this](const easel::Message& message) {
    this->suspendHandler(message);
  }).get());

  int ret = mComm->OpenPersistent(easel::EASEL_SERVICE_SYSCTRL, /*logging=*/true);
  // Code should not reach here
  LOG(ERROR) << "OpenPersistent() returned unexpectedly: " << strerror(-ret);
}

void EaselPowerServerBlue::suspendHandler(const easel::Message& message) {
    CHECK_EQ(SUSPEND_CHANNEL, message.GetChannelId()) << "invalid channel id";

    std::lock_guard<std::mutex> lock(mSuspendLock);

    suspend_count_++;
    LOG(INFO) << "suspend channel: got message #" << suspend_count_;

    // entry point to suspend the kernel
    int fd = open(KERNEL_SUSPEND_SYS_FILE, O_WRONLY);
    if (fd == -1) {
      PLOG(ERROR) << "failed to open " <<  KERNEL_SUSPEND_SYS_FILE << ", abort suspending";
      return;
    }

    const char *buf = KERNEL_SUSPEND_STRING;
    ssize_t bytes = write(fd, buf, strlen(buf));  // should have suspended before write() returns
    if (bytes == -1) {
      PLOG(ERROR) << "error writing " << buf << " to " << KERNEL_SUSPEND_SYS_FILE;
      LOG(WARNING) << "Easel may have not suspended";
    }
    close(fd);
}

} // namespace EaselPowerBlue
} // namespace android
