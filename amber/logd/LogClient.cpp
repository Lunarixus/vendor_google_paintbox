#define LOG_TAG "LogClient"

#include "LogClient.h"

#include <errno.h>
#include <log/log.h>
#include <log/log_read.h>

#include <string>

#include "LogMessage.h"
#include "LogEntry.h"

static const int OPEN_TIMEOUT_MS = 2000;

namespace EaselLog {

LogClient::LogClient() {}

LogClient::~LogClient() {
  stop();
}

int LogClient::start() {
  if (mReceivingThread.joinable() || mCommClient.isConnected()) {
    return -EBUSY;
  }
  mReceivingThread = std::thread(&LogClient::receiveLogThread, this);

  return 0;
}

void LogClient::waitForReadyToReceive() {
  if (mReceivingThread.joinable()) {
    mReceivingThread.join();
  }
}

void LogClient::stop() {
  if (mReceivingThread.joinable()) {
    mReceivingThread.join();
  }
  if (mCommClient.isConnected()) {
    mCommClient.close();
  }
}

// Running in mReceivingThread.
// mCommClient is opened async to save camera boot time.
void LogClient::receiveLogThread() {
  int ret = mCommClient.open(EASEL_SERVICE_LOG, OPEN_TIMEOUT_MS);
  if (ret != 0) {
    ALOGE("open easelcomm client error (%d, %d), "
        "did you have two LogClient running at the same time? "
        "e.g. ezlsh and camera app", ret, errno);
  } else {
    ret = mCommClient.startMessageHandlerThread(
        [this](EaselComm::EaselMessage *msg) {
      char textBuf[LOGGER_ENTRY_MAX_PAYLOAD];

      LogMessage* logMsg = reinterpret_cast<LogMessage*>(msg->message_buf);

      log_id_t logId = logMsg->log_id;
      char* log = logMsg->log;

      LogEntry entry = parseEntry(log, logMsg->len);

      // Logs PID, TID and text for easel.
      // Every easel log will have a prefix of EASEL
      // on the first line for debugging purpose.
      // If text is too long, it may gets truncated.
      snprintf(textBuf, LOGGER_ENTRY_MAX_PAYLOAD - (entry.text - log),
          "EASEL (%02u:%02u.%03u P%d T%d): %s",
          static_cast<unsigned>(((LOG_TIME_SEC(&logMsg->realtime) / 60) % 60)),
          static_cast<unsigned>((LOG_TIME_SEC(&logMsg->realtime) % 60)),
          static_cast<unsigned>((LOG_TIME_NSEC(&logMsg->realtime) * MS_PER_SEC / NS_PER_SEC)),
          static_cast<int>(logMsg->pid),
          static_cast<int>(logMsg->tid), entry.text);

      __android_log_buf_write(logId, entry.prio, entry.tag, textBuf);
    });
    if (ret != 0) {
      ALOGE("could not start log thread error (%d)", ret);
    }
  }
}

}  // namespace EaselLog
