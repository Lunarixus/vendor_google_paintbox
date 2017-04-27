#include <stdio.h>
#include <stdlib.h>

#include "LogBufferEasel.h"
#include "LogMessage.h"

LogBufferEasel::LogBufferEasel() {
  mCommServer.open(EaselComm::EASEL_SERVICE_LOG);
}

LogBufferEasel::~LogBufferEasel() {
  mCommServer.close();
}

int LogBufferEasel::log(
      log_id_t log_id, log_time realtime, uid_t uid, pid_t pid, pid_t tid,
      const char* msg, unsigned short len) {
  LogMessage logMessage(log_id, realtime, uid, pid, tid, msg, len);

  EaselComm::EaselMessage easelMsg;
  easelMsg.message_buf = reinterpret_cast<void*>(&logMessage);
  easelMsg.message_buf_size = logMessage.size();
  easelMsg.dma_buf = 0;
  easelMsg.dma_buf_size = 0;
  int ret = mCommServer.sendMessage(&easelMsg);
  if (ret != 0) {
    fprintf(stderr, "Could not send log errno %d.\n", errno);
  }
  return len;
}
