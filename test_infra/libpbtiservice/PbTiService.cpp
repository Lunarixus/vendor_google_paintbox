// #define LOG_NDEBUG 0
#define LOG_TAG "PbTiService"

#include <sys/wait.h>
#include <unistd.h>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iterator>

#include "Log.h"
#include "PbTiService.h"

namespace pbti {

PbTiService::PbTiService() {
}

PbTiService::~PbTiService() {
    std::unique_lock<std::mutex> lock(mApiLock);
    stopLocked();
}

status_t PbTiService::start() {
    std::unique_lock<std::mutex> lock(mApiLock);

    // Opening Easel Control
    status_t res = mEaselControl.open();
    if (res != 0) {
        ALOGE("%s: Opening Easel Control failed: %s (%d).",
              __FUNCTION__, strerror(-errno), errno);
        stopLocked();
        return -ENODEV;
    }

    res = mMessengerToClient.connect(*this);
    if (res != 0) {
        ALOGE("%s: Connecting to messenger failed: %s (%d).",
              __FUNCTION__, strerror(-res), res);
        stopLocked();
        return -ENODEV;
    }

    return 0;
}

status_t PbTiService::stop() {
    // The method is not implemented.
    return 0;
}

void PbTiService::stopLocked() {
    mMessengerToClient.disconnect();
    mEaselControl.close();
    mExitCondition.notify_one();
}

void PbTiService::wait() {
    std::unique_lock<std::mutex> lock(mApiLock);
    mExitCondition.wait(lock);
}

status_t PbTiService::connect() {
    ALOGD("%s: Connected.", __FUNCTION__);
    return 0;
}

void PbTiService::disconnect() {
    ALOGD("%s: Disconnected.", __FUNCTION__);
}

void appendMessegeToLog(const std::string &logFile, const std::string &msg) {
    std::ofstream outfile;
    outfile.open(logFile, std::ios_base::app);
    outfile << msg;
}

status_t PbTiService::submitPbTiTestRequest(const PbTiTestRequest &request) {
    std::unique_lock<std::mutex> lock(mApiLock);

    std::string cmd = "rm -f " + request.log_path + " && " +
                      request.test_command + " &> " + request.log_path;
    ALOGD("Executing: %s", cmd.c_str());
    int pid = fork();
    clock_t begin = clock();
    if (pid == -1) {
        ALOGE("Creating new process by fork() failed!");
        return -1;
    }

    if (pid == 0) {
        // code block for child process
        execl("/bin/sh", "sh", "-c", cmd.c_str(), NULL);
    }

    // If elapsed time is less than timeout seconds and pid is still running.
    // we have to wait.
    int status;
    while (
      static_cast<int>(clock() - begin) / CLOCKS_PER_SEC <= request.timeout_seconds &&
      waitpid(pid, &status, WNOHANG) == 0) {
        sleep(2);
    }

    // Timed out, kill the child process
    if (static_cast<int>(clock() - begin) / CLOCKS_PER_SEC > request.timeout_seconds) {
        kill(pid, SIGTERM);
        std::string errMsg = "ERROR: Command <" + cmd + "> is timed out!";
        ALOGE("%s", errMsg.c_str());
        // Append error message to log file so that AP can parse it
        appendMessegeToLog(request.log_path, errMsg);
    }

    if (!WIFEXITED(status)) {
        std::string errMsg = "ERROR: Test process is not terminated normally!";
        ALOGE("%s", errMsg.c_str());
        // Append error message to log file so that AP can parse it
        appendMessegeToLog(request.log_path, errMsg);
    }

    ALOGD("Done.");
    appendMessegeToLog(request.log_path, "TEST SUCCESS");

    // Send the test log file to client.
    mMessengerToClient.notifyPbTiTestResult(request.log_path);

    // Still return 0 to let service alive all time
    return 0;
}

}  // namespace pbti
