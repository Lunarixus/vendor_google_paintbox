#define LOG_TAG "LogClient"

#include "LogClient.h"

#include <errno.h>
#include <log/log.h>
#include <log/log_read.h>

#include <string>

#include "LogMessage.h"

namespace EaselLog {

LogClient::LogClient() {
  mFinish = true;
}

LogClient::~LogClient() {
  stop();
}

int LogClient::start() {
  if (!mFinish) {
    return -EBUSY;
  }

  mFinish = false;
  if (mReceivingThread.joinable()) {
    ALOGE("LogClient: receiving thread already started");
    mFinish = true;
    return -EBUSY;
  }

  mReceivingThread = std::thread(&LogClient::log, this);

  return 0;
}

void LogClient::stop() {
  mFinish = true;
  std::lock_guard<std::mutex> lock(mClientGuard);
  mCommClient.close();
  if (mReceivingThread.joinable()) {
    mReceivingThread.join();
  }
}

// Running in mReceivingThread
void LogClient::log() {
  int ret = 0;
  {
    // Open easel comm client in thread to save camera boot time.
    std::lock_guard<std::mutex> lock(mClientGuard);
    ret = mCommClient.open(EaselComm::EASEL_SERVICE_LOG);
  }

  if (ret != 0) {
    ALOGE("open easelcomm client error (%d, %d), exiting", ret, errno);
    mFinish = true;
    return;
  }

  EaselComm::EaselMessage msg;
  char textBuf[LOGGER_ENTRY_MAX_PAYLOAD];
  while (!mFinish) {
    int ret = mCommClient.receiveMessage(&msg);
    if (ret != 0) {
      if (errno != ESHUTDOWN) {
        ALOGE("receiveMessage error (%d, %d), exiting", ret, errno);
      } else {
        ALOGI("server shutdown, exiting");
      }
      break;
    }

    if (msg.message_buf == nullptr) {
      continue;
    }

    LogMessage* logMsg = reinterpret_cast<LogMessage*>(msg.message_buf);

    log_id_t logId = logMsg->log_id;
    char* log = logMsg->log;
    int prio = *log;
    const char* tag = log + 1;
    const char* text = tag + strnlen(tag, logMsg->len - 1) + 1;

    // Logs PID, TID and text for easel.
    // Every easel log will have a prefix of EASEL
    // on the first line for debugging purpose.
    // If text is too long, it may gets truncated.
    // TODO(cjluo): Handle realtime if needed.
    snprintf(textBuf, LOGGER_ENTRY_MAX_PAYLOAD - (text - log),
        "EASEL (PID %d TID %d): %s",
        static_cast<int>(logMsg->pid),
        static_cast<int>(logMsg->tid), text);

    __android_log_buf_write(logId, prio, tag, textBuf);

    free(msg.message_buf);
  }
}

}  // namespace EaselLog
