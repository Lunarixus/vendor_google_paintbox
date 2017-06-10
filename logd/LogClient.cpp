#define LOG_TAG "LogClient"

#include "LogClient.h"

#include <errno.h>
#include <log/log.h>
#include <log/log_read.h>

#include <string>

#include "LogMessage.h"
#include "LogEntry.h"

namespace EaselLog {

LogClient::LogClient() {
  mState = LogClientState::STOPPED;
}

LogClient::~LogClient() {
  stop();
}

int LogClient::start() {
  {
    std::lock_guard<std::mutex> lock(mClientGuard);
    if (mState != LogClientState::STOPPED) {
      return -EBUSY;
    }
    mState = LogClientState::STARTING;
  }

  mReceivingThread = std::thread(&LogClient::receiveLogThread, this);

  return 0;
}

void LogClient::stop() {
  std::unique_lock<std::mutex> lock(mClientGuard);
  if (mState == LogClientState::STOPPING || mState == LogClientState::STOPPED) {
    return;
  }
  mStarted.wait(lock, [this]{return mState == LogClientState::STARTED;});
  mState = LogClientState::STOPPING;
  mCommClient.close();
  if (mReceivingThread.joinable()) {
    mReceivingThread.join();
  }
  mState = LogClientState::STOPPED;
}

// Running in mReceivingThread.
// mCommClient is opened async to save camera boot time.
void LogClient::receiveLogThread() {
  int ret = 0;

  {
    std::lock_guard<std::mutex> lock(mClientGuard);

    if (mState == LogClientState::STARTING) {
      ret = mCommClient.open(EaselComm::EASEL_SERVICE_LOG);
      if (ret != 0) {
        ALOGE("open easelcomm client error (%d, %d)", ret, errno);
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
          // TODO(cjluo): Handle realtime if needed.
          snprintf(textBuf, LOGGER_ENTRY_MAX_PAYLOAD - (entry.text - log),
              "EASEL (PID %d TID %d): %s",
              static_cast<int>(logMsg->pid),
              static_cast<int>(logMsg->tid), entry.text);

          __android_log_buf_write(logId, entry.prio, entry.tag, textBuf);
        });
        if (ret != 0) {
          ALOGE("could not start log thread error (%d)", ret);
        }
      }
    }

    // Mark status to be started regardless of ret value,
    // so it could be properly stopped.
    mState = LogClientState::STARTED;
  }

  mStarted.notify_one();
}

}  // namespace EaselLog
