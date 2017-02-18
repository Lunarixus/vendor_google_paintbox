#include <utils/Log.h>

#include <getopt.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>

#include "pbticlientrunner.h"

PbTiClientRunner::PbTiClientRunner() : mConnected(false) {
}

PbTiClientRunner::~PbTiClientRunner() {
    if (mConnected) {
        mClient.disconnect();
    }
}

void PbTiClientRunner::onPbTiTestResult(const std::string &result) {
    // Get test log file to /data/local/tmp
    const std::string ap_log_dir = "/data/local/tmp/";
    const std::string cmd("ezlsh pull " + result + " " + ap_log_dir + result);
    system(cmd.c_str());
    ALOGI("Log file: %s", (ap_log_dir + result).c_str());
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
    std::cout << "Usage: pbticlient [-c TEST_COMMAND] [-l LOG_PATH] "
              << "[-t TIMEOUT_SECONDS]\n"
              << "Arguments: \n"
              << "  -c, --test_command \t command line to run tests on easel \n"
              << "  -l, --test_path \t test log path on Easel \n"
              << "  -t, --timeout_seconds \t timeout seconds"
              << std::endl;
}

int parse_args(int argc, const char **argv, pbti::PbTiTestRequest &request) {
    const struct option long_options[] = {
        { "help",              no_argument,       0, 'h' },
        { "test_command",      required_argument, 0, 'c' },
        { "log_path",          required_argument, 0, 'l' },
        { "timeout_seconds",   required_argument, 0, 't' },
        { 0,                   0,                 0, 0   }
    };

    while (true) {
        // getopt_long stores the option index here.
        int option_index = 0;

        int option_val = getopt_long(argc, (char * const *)(argv),
                                     "hc:l:t:", long_options, &option_index);

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
            break;
        case 'c':
            request.test_command = optarg;
            break;
        case 'l':
            request.log_path = optarg;
            break;
        case 't':
            request.timeout_seconds = atoi(optarg);
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
    pbti::PbTiTestRequest request;
    int ret = parse_args(argc, argv, request);
    if (ret != 0) {
        usage();
    }

    ALOGD("Test command: %s", request.test_command.c_str());
    ALOGD("Log path: %s", request.log_path.c_str());
    ALOGD("Timeout seconds: %d", request.timeout_seconds);

    std::unique_ptr<PbTiClientRunner> client_runner =
            std::make_unique<PbTiClientRunner>();

    int res =client_runner->connectClient();
    if (res != 0) {
        return res;
    }
    res = client_runner->submitPbTiTestRequest(request);

    return res;
}
