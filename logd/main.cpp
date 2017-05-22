#include <stdlib.h>
#include <syslog.h>

#include <cutils/android_get_control_file.h>

#include "LogBufferEasel.h"
#include "LogListener.h"
#include "LogUtils.h"

#define KMSG_PRIORITY(PRI)                                 \
    '<', '0' + LOG_MAKEPRI(LOG_DAEMON, LOG_PRI(PRI)) / 10, \
        '0' + LOG_MAKEPRI(LOG_DAEMON, LOG_PRI(PRI)) % 10, '>'

static LogBufferInterface* logBuf = nullptr;

char* android::uidToName(uid_t) {
    return nullptr;
}

void android::prdebug(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

int main() {

    // LogBuffer is the object which is responsible for holding all
    // log entries.

    logBuf = new LogBufferEasel();

    // LogListener listens on /dev/socket/logdw for client
    // initiated log messages. New log entries are added to LogBuffer
    // and LogReader is notified to send updates to connected clients.

    LogListener* swl = new LogListener(logBuf, nullptr);
    // Backlog and /proc/sys/net/unix/max_dgram_qlen set to large value
    if (swl->startListener(64)) {
        exit(1);
    }

    TEMP_FAILURE_RETRY(pause());

    exit(0);
}
