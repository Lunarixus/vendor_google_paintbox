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

static int fdDmesg = -1;
void android::prdebug(const char* fmt, ...) {
    if (fdDmesg < 0) {
        return;
    }

    static const char message[] = {
        KMSG_PRIORITY(LOG_DEBUG), 'l', 'o', 'g', 'd', ':', ' '
    };
    char buffer[256];
    memcpy(buffer, message, sizeof(message));

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buffer + sizeof(message),
                      sizeof(buffer) - sizeof(message), fmt, ap);
    va_end(ap);
    if (n > 0) {
        buffer[sizeof(buffer) - 1] = '\0';
        if (!strchr(buffer, '\n')) {
            buffer[sizeof(buffer) - 2] = '\0';
            strlcat(buffer, "\n", sizeof(buffer));
        }
        write(fdDmesg, buffer, strlen(buffer));
    }
}

int main() {
    static const char dev_kmsg[] = "/dev/kmsg";
    fdDmesg = android_get_control_file(dev_kmsg);
    if (fdDmesg < 0) {
        fdDmesg = TEMP_FAILURE_RETRY(open(dev_kmsg, O_WRONLY | O_CLOEXEC));
    }

    // LogBuffer is the object which is responsible for holding all
    // log entries.

    logBuf = new LogBufferEasel();

    // LogListener listens on /dev/socket/logdw for client
    // initiated log messages. New log entries are added to LogBuffer
    // and LogReader is notified to send updates to connected clients.

    LogListener* swl = new LogListener(logBuf, nullptr);
    // Backlog and /proc/sys/net/unix/max_dgram_qlen set to large value
    if (swl->startListener(600)) {
        exit(1);
    }

    TEMP_FAILURE_RETRY(pause());

    exit(0);
}
