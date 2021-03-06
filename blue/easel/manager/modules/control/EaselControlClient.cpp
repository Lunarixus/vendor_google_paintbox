#define LOG_TAG "EaselControlClient"

#include <android/log.h>
#include <cutils/properties.h>
#include <log/log.h>

#include <poll.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fstream>
#include <future>
#include <streambuf>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "EaselStateManager.h"
#include "EaselThermalMonitor.h"
#include "EaselTimer.h"
#include "easelcontrol.h"
#include "easelcontrol_impl.h"
#include "easelcomm.h"
#include "LogClient.h"

#define ESM_DEV_FILE    "/dev/mnh_sm"
#define NSEC_PER_SEC    1000000000ULL
#define NSEC_PER_MSEC   1000000
#define NSEC_PER_USEC   1000
#define ESM_EVENT_PATH  "/sys/devices/virtual/misc/mnh_sm/error_event"

namespace {
EaselCommClient easel_conn;

// EaselStateManager instance
EaselStateManager stateMgr;

std::thread conn_thread;

EaselService serviceId;

// Indicate an activate commmand is pending
std::atomic_flag isActivatePending = ATOMIC_FLAG_INIT;  // initializes to false

EaselLog::LogClient gLogClient;

// Error callback registered by user
easel_error_callback_t gErrorCallback;

const int controlChannelReplyTimeoutMs = 2000;

int defaultErrorCallback(enum EaselErrorReason r, enum EaselErrorSeverity s) {
  ALOGD("%s: Skip handling %s error (reason %d)", __FUNCTION__,
        s == EaselErrorSeverity::FATAL ? "fatal" : "non-fatal", r);
  return 0;
}

// Mutex to protect the current state of EaselControlClient
std::mutex state_mutex;
enum ControlState {
  INIT,        // Unknown initial state
  SUSPENDED,   // Suspended
  RESUMED,     // Powered, support Bypass
  PARTIAL,     // Powered, but boot failed and can only support Bypass
  ACTIVATED,   // Powered, ready for HDR+
  FAILED,      // Fatal error, wait for device close
} state = INIT;

// EaselThermalMonitor instance
EaselThermalMonitor thermalMonitor;
static const std::vector<struct EaselThermalMonitor::Configuration>
    thermalCfg = {
  {"bcm15602_tz",    1, {60000, 70000, 80000}},
  {"s2mpb04_tz",     1, {60000, 70000, 80000}},
  {"bd_therm",    1000, {45000, 50000, 55000}}, /* for taimen */
  {"back_therm",  1000, {45000, 50000, 55000}}, /* for muskie */
};

EaselTimer watchdog;
const std::chrono::milliseconds watchdogTimeout =
    std::chrono::milliseconds(2500);
static uint32_t heartbeatSeqNumber;

}  // anonymous namespace

int stopWatchdog();

/*
 * Determine severity
 *
 * | Reason              |  RESUMED  | ACTIVATED |
 * |---------------------|-----------|-----------|
 * | LINK_FAIL           |   FATAL   |   FATAL   |
 * | BOOTSTRAP_FAIL      | NON_FATAL |   FATAL   |
 * | OPEN_SYSCTRL_FAIL   | NON_FATAL |   FATAL   |
 * | HANDSHAKE_FAIL      | NON_FATAL |   FATAL   |
 * | IPU_RESET_REQ       | NON_FATAL |   FATAL   |
 * | WATCHDOG            | NON_FATAL |   FATAL   |
 */

static void reportError(enum EaselErrorReason reason) {
  enum EaselErrorSeverity severity;

  // Acquire state lock while handling error
  {
    std::unique_lock<std::mutex> state_lock(state_mutex);

    if (state == RESUMED) {
      // LINK_FAIL is fatal in bypass mode, because MIPI configuration
      // will not continue.  Others are not fatal, because no further
      // communication needed in bypass mode.
      if (reason != EaselErrorReason::LINK_FAIL) {
        severity = EaselErrorSeverity::NON_FATAL;
        state = ControlState::PARTIAL;
      } else {
        severity = EaselErrorSeverity::FATAL;
        // Watchdog should not be stopped during timer callback. Since
        // watchdog is a oneshot timer, we don't need to explicitly stop it.
        if (reason != EaselErrorReason::WATCHDOG) {
          stopWatchdog();
        }
        state = ControlState::FAILED;
      }
    } else {
      // All errors are fatal in HDR+ mode
      severity = EaselErrorSeverity::FATAL;
      // Watchdog should not be stopped during timer callback. Since
      // watchdog is a oneshot timer, we don't need to explicitly stop it.
      if (reason != EaselErrorReason::WATCHDOG) {
        stopWatchdog();
      }
      state = ControlState::FAILED;
    }
  }

  int ret = gErrorCallback(reason, severity);
  if (!ret) {
    ALOGD("%s: Fatal error callback was handled", __FUNCTION__);
  } else {
    ALOGE("%s: Fatal error callback was not well handled (%d)",
          __FUNCTION__, ret);
  }
}

static void reportErrorAsync(enum EaselErrorReason reason) {
  std::thread([reason](){ reportError(reason); }).detach();
}

static int sendTimestamp(void) {
  ALOGD("%s\n", __FUNCTION__);

  // Prepare local timestamp and send to server
  EaselControlImpl::SetTimeMsg ctrl_msg;
  ctrl_msg.h.command = EaselControlImpl::CMD_SET_TIME;
  struct timespec ts;
  clock_gettime(CLOCK_BOOTTIME, &ts);
  ctrl_msg.boottime = (uint64_t)ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
  clock_gettime(CLOCK_REALTIME, &ts);
  ctrl_msg.realtime = (uint64_t)ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;

  EaselComm::EaselMessage msg;
  msg.message_buf = &ctrl_msg;
  msg.message_buf_size = sizeof(ctrl_msg);
  msg.dma_buf = 0;
  msg.dma_buf_size = 0;
  msg.need_reply = true;
  msg.timeout_ms = controlChannelReplyTimeoutMs;

  int replycode;
  EaselComm::EaselMessage reply;

  int ret = easel_conn.sendMessageReceiveReply(&msg, &replycode, &reply);
  if (ret) {
    ALOGE("%s: Failed to send timestamp (%d)\n", __FUNCTION__, ret);
    return ret;
  }

  if (replycode != EaselControlImpl::REPLY_SET_TIME_OK) {
    ALOGE("%s: Failed to receive SET_TIME_OK (%d)\n",
          __FUNCTION__, replycode);
    return ret;
  }

  // Get timestamp returned by server
  EaselControlImpl::SetTimeMsg *tmsg =
      (EaselControlImpl::SetTimeMsg *)reply.message_buf;

  // Check local timestamp again
  struct timespec new_ts;
  clock_gettime(CLOCK_REALTIME, &new_ts);
  uint64_t realtime = (uint64_t)new_ts.tv_sec * NSEC_PER_SEC + new_ts.tv_nsec;

  ALOGD("%s: Server timestamp is %ld us behind (oneway)\n" , __FUNCTION__,
        ((long)realtime - (long)tmsg->realtime) / NSEC_PER_USEC);
  ALOGD("%s took %ld us\n" , __FUNCTION__,
        ((long)realtime - (long)ctrl_msg.realtime) / NSEC_PER_USEC);

  return ret;
}

static void captureBootTrace() {
  std::ifstream t("/sys/devices/virtual/misc/mnh_sm/boot_trace");
  std::string str((std::istreambuf_iterator<char>(t)),
      std::istreambuf_iterator<char>());
  if (!str.empty() && str[str.length()-1] == '\n') {
    str.erase(str.length()-1);
  }
  ALOGE("%s: Boot trace = [%s]\n", __FUNCTION__, str.c_str());
  t.close();
}

void EventReportingThread(int pipeReadFd) {
  int fd = ::open(ESM_EVENT_PATH, O_RDONLY);
  if (fd < 0) {
    ALOGE("%s: failed to open event reporting file (%d)", __FUNCTION__, -errno);
    ::close(pipeReadFd);
    return;
  }

  // Do a dummy ready to clear poll status
  char value;
  read(fd, &value, 1);

  // Create the poll structure
  struct pollfd pollFds[2];
  pollFds[0].fd = fd;
  pollFds[0].events = 0;
  pollFds[0].revents = 0;

  // This fd is used to signal to the thread to exit the loop
  pollFds[1].fd = pipeReadFd;
  pollFds[1].events = POLLIN;
  pollFds[1].revents = 0;

  while (1) {
    int ret;

    do {
      ret = poll(pollFds, 2, -1);
    } while (ret < 0 && errno == EINTR);

    if (pollFds[0].revents & POLLERR) {
      // Read from the sysfs file to reset the poll status, but the value
      // is not important since the only event we report is a link
      // failure.
      char value;
      lseek(pollFds[0].fd, 0, SEEK_SET);
      read(pollFds[0].fd, &value, 1);

      ALOGE("%s: observed link failure", __FUNCTION__);
      reportErrorAsync(EaselErrorReason::LINK_FAIL);
    }

    if (pollFds[1].revents & POLLIN) {
      break;
    }
  }

  ::close(pipeReadFd);
  ::close(fd);
}

void setActivatePending() {
  ALOGV("%s", __FUNCTION__);
  isActivatePending.test_and_set();
}

void clearActivatePending() {
  ALOGV("%s", __FUNCTION__);
  isActivatePending.clear();
}

int sendActivateCommand() {
  EaselControlImpl::ActivateMsg ctrl_msg;
  ctrl_msg.h.command = EaselControlImpl::CMD_ACTIVATE;

  struct timespec ts;
  clock_gettime(CLOCK_BOOTTIME, &ts);
  ctrl_msg.boottime = (uint64_t)ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
  clock_gettime(CLOCK_REALTIME, &ts);
  ctrl_msg.realtime = (uint64_t)ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;

  EaselComm::EaselMessage msg;
  msg.message_buf = &ctrl_msg;
  msg.message_buf_size = sizeof(ctrl_msg);
  msg.dma_buf = 0;
  msg.dma_buf_size = 0;
  msg.need_reply = true;
  msg.timeout_ms = controlChannelReplyTimeoutMs;

  int replycode;

  int ret = easel_conn.sendMessageReceiveReply(&msg, &replycode, nullptr);
  if (ret) {
    ALOGE("%s: Failed to send activate message to Easel (%d)\n",
          __FUNCTION__, ret);
    return ret;
  }

  if (replycode != EaselControlImpl::REPLY_ACTIVATE_OK) {
    ALOGE("%s: Failed to receive ACTIVATE_OK (%d)\n", __FUNCTION__, replycode);
    return ret;
  }

  ret = sendTimestamp();
  if (ret) {
    ALOGE("%s: Failed to send sendTimestamp (%d)\n", __FUNCTION__, ret);
    return ret;
  }

  return 0;
}

int sendDeactivateCommand() {
  EaselControlImpl::DeactivateMsg ctrl_msg;
  ctrl_msg.h.command = EaselControlImpl::CMD_DEACTIVATE;

  EaselComm::EaselMessage msg;
  msg.message_buf = &ctrl_msg;
  msg.message_buf_size = sizeof(ctrl_msg);
  msg.dma_buf = 0;
  msg.dma_buf_size = 0;
  int ret = easel_conn.sendMessage(&msg);
  if (ret) {
    ALOGE("%s: failed to send deactivate command to Easel (%d)\n", __FUNCTION__,
          ret);
  }

  return ret;
}

// Handle incoming messages from EaselControlServer.
void msgHandlerCallback(EaselComm::EaselMessage* msg) {
  EaselControlImpl::MsgHeader *h =
      (EaselControlImpl::MsgHeader *)msg->message_buf;

  ALOGD("Received command %d", h->command);

  switch (h->command) {
  case EaselControlImpl::CMD_RESET_REQ: {
    ALOGW("Server requested a chip reset");
    reportErrorAsync(EaselErrorReason::IPU_RESET_REQ);
    break;
  }

  case EaselControlImpl::CMD_HEARTBEAT: {
    EaselControlImpl::HeartbeatMsg *heartbeatMsg =
        (EaselControlImpl::HeartbeatMsg *)msg->message_buf;
    ALOGD("%s: server heartbeat %d", __FUNCTION__, heartbeatMsg->seqNumber);
    if (heartbeatMsg->seqNumber != heartbeatSeqNumber) {
      ALOGW("%s: heartbeat sequence number did not match: %d (expected %d)",
            __FUNCTION__, heartbeatMsg->seqNumber, heartbeatSeqNumber);
    }
    heartbeatSeqNumber = heartbeatMsg->seqNumber + 1;
    watchdog.restart();
    break;
  }

  default:
    ALOGE("ERROR: unrecognized command %d", h->command);
    break;
  }
}

void easelConnThread() {
  // Wait for state manager to reach ACTIVE state, which means that Easel is
  // powered and is executing firmware. This is separate from Activated state,
  // which means EaselControlServer is running in HDR+ mode.
  ALOGD("%s: Waiting for active state", __FUNCTION__);
  int ret = stateMgr.waitForState(EaselStateManager::ESM_STATE_ACTIVE);
  if (ret) {
    captureBootTrace();
    if (ret == -EHOSTUNREACH) {
      ALOGE("%s: Easel is in a partial active state", __FUNCTION__);
      reportErrorAsync(EaselErrorReason::BOOTSTRAP_FAIL);
    } else {
      ALOGE("%s: Easel failed to enter active state (%d)\n", __FUNCTION__, ret);
      reportErrorAsync(EaselErrorReason::LINK_FAIL);
    }
    return;
  }

  ALOGI("%s: Opening easel_conn", __FUNCTION__);
  ret = easel_conn.open(serviceId);
  if (ret) {
    ALOGE("%s: Failed to open easelcomm connection (%d)", __FUNCTION__, ret);
    captureBootTrace();
    reportErrorAsync(EaselErrorReason::OPEN_SYSCTRL_FAIL);
    return;
  }

  captureBootTrace();

  easel_conn.startMessageHandlerThread(msgHandlerCallback);

  ALOGV("%s: check isActivatePending", __FUNCTION__);
  if (!isActivatePending.test_and_set()) {
    // No activate pending, go to bypass mode first; return value can be
    // ignored.
    ALOGD("%s: sending deactivate command", __FUNCTION__);
    sendDeactivateCommand();
    isActivatePending.clear();  // Clear isActivatePending flag
  }
}

int setupEaselConn() {
  if (conn_thread.joinable() || easel_conn.isConnected()) {
    return 0;
  }
  conn_thread = std::thread(easelConnThread);
  return 0;
}

int waitForEaselConn() {
  if (conn_thread.joinable()) {
    conn_thread.join();
  }
  if (!easel_conn.isConnected()) {
    return -EIO;
  }
  return 0;
}

int teardownEaselConn() {
  if (conn_thread.joinable()) {
    conn_thread.join();
  }

  easel_conn.close();

  return 0;
}

int startThermalMonitor() {
  int ret = thermalMonitor.start();
  if (ret) {
    ALOGE("failed to start EaselThermalMonitor (%d)\n", ret);
  }

  return ret;
}

int stopThermalMonitor() {
  int ret = thermalMonitor.stop();
  if (ret) {
    ALOGE("%s: failed to stop EaselThermalMonitor (%d)\n", __FUNCTION__, ret);
  }

  return ret;
}

int startLogClient() {
  int ret = gLogClient.start();
  if (ret) {
    ALOGE("Failed to start LogClient (%d)\n", ret);
  }

  return ret;
}

int stopLogClient() {
  gLogClient.stop();

  return 0;
}

// This thread is used to monitor asynchronous events from the driver
static std::thread mEventThread;

// The parent-side of the pipe for waking the reporting thread up from poll()
static int mPipeWriteFd = -1;

int startKernelEventThread() {
  // Create a pipe to communicate with the event reporting thread
  int pipeFds[2];

  if (pipe(pipeFds) == -1) {
    ALOGE("%s: failed to create a pipe (%d)", __FUNCTION__, errno);
    return -errno;
  } else {
    if (mPipeWriteFd != -1) {
      ALOGE("%s: leaked a file descriptor (%d)", __FUNCTION__, mPipeWriteFd);
    }
    mPipeWriteFd = pipeFds[1];
    mEventThread = std::thread(EventReportingThread, pipeFds[0]);
  }

  return 0;
}

int stopKernelEventThread() {
  // Close the event reporting thread by writing the global bool and sending
  // an event through the pipe
  if (mEventThread.joinable()) {
    // Dummy string to write to pipe. Value is not read, it's just a way to
    // wakeup the thread blocked on poll
    const char buf[] = "1";

    write(mPipeWriteFd, buf, strlen(buf));
    mEventThread.join();
    ::close(mPipeWriteFd);
    mPipeWriteFd = -1;
  }

  return 0;
}

int startWatchdog() {
  int ret = watchdog.start(watchdogTimeout,
                           []() {
                             reportErrorAsync(EaselErrorReason::WATCHDOG);
                           },
                           /*fireOnce=*/true);
  if (ret) {
    ALOGE("%s: failed to start watchdog (%d)\n", __FUNCTION__, ret);
  }

  heartbeatSeqNumber = 0;

  return ret;
}

int stopWatchdog() {
  return watchdog.stop();
}

int switchState(enum ControlState nextState) {
  int ret = 0;

  std::unique_lock<std::mutex> state_lock(state_mutex);

  ALOGD("%s: Switch from state %d to state %d", __FUNCTION__, state, nextState);

  if (state == nextState) {
    return 0;
  }

  switch (nextState) {
  case ControlState::SUSPENDED: {
    switch (state) {
    case ControlState::ACTIVATED:
    case ControlState::PARTIAL:
    case ControlState::FAILED:
    case ControlState::RESUMED:
    case ControlState::INIT:
      clearActivatePending();
      stopWatchdog();
      stopThermalMonitor();
      stopLogClient();
      teardownEaselConn();
      stateMgr.setState(EaselStateManager::ESM_STATE_OFF);
      stopKernelEventThread();
      break;
    default:
      ALOGE("%s: Invalid state transition from %d to %d", __FUNCTION__, state,
            nextState);
      ret = -EINVAL;
      break;
    }
    break;
  }

  case ControlState::RESUMED: {
    switch (state) {
    case ControlState::SUSPENDED:
      ret = startKernelEventThread();
      if (!ret) {
        ret = stateMgr.setState(EaselStateManager::ESM_STATE_ACTIVE, false);
      }
      if (!ret) {
        ret = setupEaselConn();
      }
      if (!ret) {
        ret = startLogClient();
      }
      if (!ret) {
        ret = startThermalMonitor();
      }
      break;
    case ControlState::ACTIVATED:
      ret = stopWatchdog();
      if (!ret) {
        ret = sendDeactivateCommand();
      }
      break;
    default:
      ALOGE("%s: Invalid state transition from %d to %d", __FUNCTION__, state,
            nextState);
      ret = -EINVAL;
      break;
    }
    break;
  }

  case ControlState::ACTIVATED: {
    switch (state) {
    case ControlState::SUSPENDED:
      ret = startKernelEventThread();
      if (!ret) {
        ret = stateMgr.setState(EaselStateManager::ESM_STATE_ACTIVE, false);
        if (!ret) {
          ret = setupEaselConn();
        }
        if (!ret) {
          ret = startLogClient();
        }
        if (!ret) {
          ret = startThermalMonitor();
        }
        if (!ret) {
          // Mark the flag so deactivate is not sent by easelconn thread
          setActivatePending();
          ret = waitForEaselConn();
        }
        if (!ret) {
          ret = sendActivateCommand();
          clearActivatePending();
        }
        if (!ret) {
          ret = startWatchdog();
        }
      }
      break;
    case ControlState::RESUMED:
      // Mark the flag so deactivate is not sent by easelconn thread
      setActivatePending();
      ret = waitForEaselConn();
      if (!ret) {
        ret = sendActivateCommand();
        clearActivatePending();
      }
      if (!ret) {
        ret = startWatchdog();
      }
      break;
    case ControlState::PARTIAL:
      // If Easel did not boot correctly, we cannot transition
      // into the ACTIVATED state. Let the upper layer decide how
      // to handle this use case.
      ret = -EIO;
      break;
    default:
      ALOGE("%s: Invalid state transition from %d to %d", __FUNCTION__, state,
            nextState);
      ret = -EINVAL;
      break;
    }
    break;
  }

  default:
    ALOGE("%s: Invalid nextState %d", __FUNCTION__, nextState);
    ret = -EINVAL;
    break;
  }

  if (ret) {
    ALOGE("%s: Failed to switch from state %d to state %d (%d)", __FUNCTION__,
          state, nextState, ret);
  } else {
    state = nextState;
  }

  return ret;
}

int EaselControlClient::activate() {
  ALOGI("%s\n", __FUNCTION__);

  int ret = switchState(ControlState::ACTIVATED);
  if (ret) {
    ALOGE("%s: failed to activate Easel (%d)\n", __FUNCTION__, ret);
  }

  return ret;
}

int EaselControlClient::deactivate() {
  ALOGI("%s\n", __FUNCTION__);

  int ret = switchState(ControlState::RESUMED);
  if (ret) {
    ALOGE("%s: failed to deactivate Easel (%d)\n", __FUNCTION__, ret);
  }

  return ret;
}

int EaselControlClient::getFwVersion(char *fwVersion) {
  int ret = stateMgr.getFwVersion(fwVersion);
  ALOGD("%s: Easel getFwVersion: %.*s code:%d",
        __FUNCTION__, FW_VER_SIZE, fwVersion, ret);

  return ret;
}

// Called when the camera app is opened
int EaselControlClient::resume() {
  ALOGD("%s\n", __FUNCTION__);

  int ret = switchState(ControlState::RESUMED);
  if (ret) {
    ALOGE("Failed to resume Easel (%d)\n", ret);
  }

  return ret;
}

// Called when the camera app is closed
int EaselControlClient::suspend() {
  ALOGD("%s\n", __FUNCTION__);

  int ret = switchState(ControlState::SUSPENDED);
  if (ret) {
    ALOGE("%s: failed to suspend Easel (%d)\n", __FUNCTION__, ret);
  }

  return ret;
}

void EaselControlClient::registerErrorCallback(easel_error_callback_t f) {
  ALOGD("%s: Callback being registered", __FUNCTION__);
  gErrorCallback = std::move(f);
}

int EaselControlClient::open(EaselService service_id) {
  serviceId = service_id;

  ALOGD("%s\n", __FUNCTION__);

  // Register default implemetation of error callback
  registerErrorCallback(defaultErrorCallback);

  int ret = thermalMonitor.open(thermalCfg);
  if (ret) {
    ALOGE("failed to open EaselThermalMonitor (%d)\n", ret);
    return ret;
  }

  ret = stateMgr.open();
  if (ret) {
    ALOGE("failed to initialize EaselStateManager (%d)\n", ret);
    return ret;
  }

  ret = switchState(ControlState::SUSPENDED);
  if (ret) {
    ALOGE("%s: failed to suspend Easel (%d)\n", __FUNCTION__, ret);
  }

  return ret;
}

void EaselControlClient::close() {
  int ret = switchState(ControlState::SUSPENDED);
  if (ret) {
    ALOGE("%s: failed to suspend Easel (%d)\n", __FUNCTION__, ret);
  }

  {
    std::lock_guard<std::mutex> state_lock(state_mutex);
    state = ControlState::INIT;
  }

  stateMgr.close();
  thermalMonitor.close();
}

bool isEaselPresent() {
  bool ret = false;
  int fd = open(ESM_DEV_FILE, O_RDONLY);

  if (fd >= 0) {
    ret = true;
    close(fd);
  }

  return ret;
}
