#include <log/log.h>

#include <getopt.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>

#include "pbticlientrunner.h"

PbTiClientRunner::PbTiClientRunner() : mEaselActivated(false),
                                       mConnected(false) {
}

PbTiClientRunner::~PbTiClientRunner() {
    if (mConnected) {
        mClient.disconnect();
        mConnected = false;
    }
}

android::status_t PbTiClientRunner::activate() {
    ALOGV("%s: activating Easel.", __FUNCTION__);

    if (mEaselActivated) {
        ALOGE("%s: Easel is already activated.", __FUNCTION__);
        return android::ALREADY_EXISTS;
    }

    android::status_t res = mClient.openEasel();
    if (res != android::OK) {
        ALOGE("%s: Easel control is not opened.", __FUNCTION__);
        return android::NO_INIT;
    }

    res = mClient.resumeEasel();
    if (res != android::OK) {
        ALOGE("%s: Easel is not resumed.", __FUNCTION__);
        return android::NO_INIT;
    }

    res = mClient.freezeEaselState();
    if (res != android::OK) {
        ALOGE("%s: Easel state is not freezed.", __FUNCTION__);
        return android::NO_INIT;
    }

    mClient.closeEasel();

    mEaselActivated = true;

    return android::OK;
}

android::status_t PbTiClientRunner::deactivate() {
    ALOGV("%s: deactivating Easel.", __FUNCTION__);

    android::status_t res = mClient.openEasel();
    if (res != android::OK) {
        ALOGE("%s: Easel control is not opened.", __FUNCTION__);
        return android::NO_INIT;
    }

    res = mClient.unfreezeEaselState();
    if (res != android::OK) {
        ALOGE("%s: Easel state is not unfreezed.", __FUNCTION__);
        return android::NO_INIT;
    }

    res = mClient.suspendEasel();
    if (res != android::OK) {
        ALOGE("%s: Easel is not suspended.", __FUNCTION__);
        return android::NO_INIT;
    }

    mClient.closeEasel();

    mEaselActivated = false;

    return android::OK;
}

void PbTiClientRunner::wait() {
    std::unique_lock<std::mutex> lock(mExitLock);
    mExitCondition.wait(lock);
}

void PbTiClientRunner::onPbTiTestResult(const std::string &result) {
    if (!result.empty()) {
        // Get log file from Easel to AP
        ALOGI("Log file: %s", result.c_str());
        system(("mkdir -p $(dirname " + result + ")").c_str());
        system(("ezlsh pull " + result + " " + result).c_str());
    }
    mExitCondition.notify_all();
}

void PbTiClientRunner::onPbTiTestResultFailed() {
    ALOGE("%s: Failed to get test result.", __FUNCTION__);
    mExitCondition.notify_all();
}

android::status_t PbTiClientRunner::connectClient() {
    int res = mClient.connect(this);
    if (res != 0) {
        ALOGE("%s: Connecting client failed: %s (%d).",
              __FUNCTION__, strerror(-res), res);
        return res;
    }

    mConnected = true;

    return 0;
}

android::status_t PbTiClientRunner::submitPbTiTestRequest(
  const pbti::PbTiTestRequest &request) {
    return mClient.submitPbTiTestRequest(request);
}

void usage() {
    std::string usage = std::string("Usage: ") +
        "pbticlient [-a ACTIVATE] [-d DEACTIVATE] [-c TEST_COMMAND] "
        "[-l LOG_PATH] [-t TIMEOUT_SECONDS]\n" +
        "Arguments: \n" +
        "  -a, --activate          activate easel \n" +
        "  -d, --deactivate        deactivate easel \n" +
        "  -c, --command           command line to run tests on easel \n" +
        "  -l, --test_path         test log path on Easel \n" +
        "  -t, --timeout_seconds   timeout seconds \n";
    std::cout << usage;
    ALOGE("%s", usage.c_str());
}

int parse_args(int argc, const char **argv, bool *activate, bool *deactivate,
               pbti::PbTiTestRequest *request) {
    const struct option long_options[] = {
        { "help",              no_argument,       0, 'h' },
        { "activate",          no_argument,       0, 'a' },
        { "deactivate",        no_argument,       0, 'd' },
        { "command",           required_argument, 0, 'c' },
        { "log_path",          required_argument, 0, 'l' },
        { "timeout_seconds",   required_argument, 0, 't' },
        { 0,                   0,                 0, 0   }
    };

    while (true) {
        // getopt_long stores the option index here.
        int option_index = 0;
        int option_val = getopt_long(argc, (char * const *)(argv),
                                     "hadc:l:t:", long_options, &option_index);

        // Detect the end of the options.
        if (option_val == -1) {
            break;
        }

        switch (option_val) {
        case 0:
            std::cout << "option "
                      << std::string(long_options[option_index].name);
            if (optarg) {
                std::cout << " with arg " << optarg;
            }
            std::cout << std::endl;
          break;
        case 'h':
            usage();
            exit(0);
        case 'a':
            *activate = true;
            break;
        case 'd':
            *deactivate = true;
            break;
        case 'c':
            (*request).command = optarg;
            break;
        case 'l':
            (*request).log_path = optarg;
            break;
        case 't':
            (*request).timeout_seconds = atoi(optarg);
            break;
        case '?':
            // getopt_long already printed an error message.
            break;
        default:
            return -1;
        }
    }

    // Print any remaining command line arguments (not options).
    if (optind < argc) {
        std::cout << "non-option ARGV-elements: ";
        while (optind < argc) {
            std::cout << argv[optind++];
        }
        std::cout << std::endl;

        return -1;
    }

    return 0;
}

int main(int argc, const char *argv[]) {
    const uint DEFAULT_TIMEOUT_SECONDS = 3;

    pbti::PbTiTestRequest request;
    bool activate = false, deactivate = false;
    int ret = parse_args(argc, argv, &activate, &deactivate, &request);
    if (ret != 0) {
        usage();
        exit(-1);
    }

    std::unique_ptr<PbTiClientRunner> client_runner =
            std::make_unique<PbTiClientRunner>();

    if (activate) {
        ret = client_runner->activate();
    } else if (deactivate) {
        ret = client_runner->deactivate();
    } else {
        if (request.command.empty()) {
            usage();
            exit(-1);
        }
        if (request.timeout_seconds == 0) {
            request.timeout_seconds = DEFAULT_TIMEOUT_SECONDS;
        }

        ALOGD("Command: %s", request.command.c_str());
        if (!request.log_path.empty()) {
            ALOGD("Log path: %s", request.log_path.c_str());
        }
        ALOGD("Timeout seconds: %d", request.timeout_seconds);

        ret = client_runner->connectClient();
        if (ret != 0) {
            return ret;
        }
        ret = client_runner->submitPbTiTestRequest(request);
        if (ret == 0) {
            client_runner->wait();
        }
    }

    return ret;
}
