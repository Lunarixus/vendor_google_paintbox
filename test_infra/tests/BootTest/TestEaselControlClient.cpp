#define LOG_TAG "EaselBootTest"

#include <getopt.h>
#include <time.h>
#include <utils/Log.h>

#include "easelcontrol.h"

namespace android {

int body(int sec, int iterations) {
    int ret = 0;
    int i;
    EaselControlClient easelControl;

    ALOGI("Start testing Easel Boot for %d cycles, staying %d secs each.",
          iterations, sec);

    struct timespec begin;
    struct timespec now;
    int64_t diff_ms;

    for (i = 1; i < iterations + 1; i++) {
        ret = easelControl.open();
        if (ret) {
            ALOGE("easelControl.open() failed (%d)", ret);
            break;
        }

        ret = easelControl.suspend();
        if (ret) {
            ALOGE("easelControl.suspend() failed (%d)", ret);
            break;
        }

        // Start timer
        clock_gettime(CLOCK_MONOTONIC, &begin);

        ret = easelControl.resume();
        if (ret) {
            ALOGE("easelControl.resume() failed (%d)", ret);
            break;
        }

        ret = easelControl.activate();
        if (ret) {
            ALOGE("easelControl.activate() failed (%d)", ret);
            break;
        }

        // Stop timer
        clock_gettime(CLOCK_MONOTONIC, &now);
        diff_ms = (now.tv_sec - begin.tv_sec) * 1000
                  + (now.tv_nsec - begin.tv_nsec) / 1000000;
        ALOGI("iter %d: Easel resume->activate done: %ld ms!", i, diff_ms);

        if (sec >= 0) {
            sleep(sec);
        } else {
            getchar();
        }

        ret = easelControl.deactivate();
        if (ret) {
            ALOGE("easelControl.deactivate() failed (%d)", ret);
            break;
        }

        ret = easelControl.suspend();
        if (ret) {
            ALOGE("Warning: easelControl.suspend() returned (%d)", ret);
            break;
        }

        easelControl.close();

        ALOGI("Testing Easel Boot n. %d done", i);

        if (sec >= 0) {
            sleep(sec);
        } else {
            getchar();
        }
    }

    if (ret == 0) {
        ALOGI("Easel Boot Test PASSED %d cycles", i - 1);
    } else {
        ALOGI("Easel Boot Test FAILED at %d cycles", i);
    }

    return ret;
}

}   // namespace android

int main(int argc, char **argv) {
    int ch;
    int numIteration = 1;
    int numSleepSecond = 1;
    int ret = 0;

    /*
     * h - help, no argument
     * i: - iteration, requires argument
     * s: - sleep seconds, requires argument
     */
    const char *short_opt = "hi:s:";
    struct option long_opt[] = {
          {"help",      no_argument,         0, 'h'},
          {"iteration", required_argument,   0, 'i'},
          {"sleep",     required_argument,   0, 's'},
          {0, 0, 0, 0}
        };

    while ((ch = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1) {
        switch (ch) {
          case 0:
            /* getopt_long() set a value */
            break;
          case 'i':
            numIteration = atoi(optarg);
            break;
          case 's':
            numSleepSecond = atoi(optarg);
            break;
          case 'h':
            /* FALLTHRU */
          default:
            fprintf(stderr,
                    "Usage: %s -s <sleep_sec> -i <iter>\n", argv[0]);
            fprintf(stderr,
                    "       -h              This help.\n"
                    "       -s <sleep_sec>  Num of seconds. \n"
                    "                       -1 to wait until user input.\n"
                    "       -i <iter>       Num of iterations.\n"
                    );
            exit(1);
        }
    }

    ret = android::body(numSleepSecond, numIteration);
    if (ret == 0) {
        fprintf(stderr, "%s PASSED\n", argv[0]);
    } else {
        fprintf(stderr, "%s FAILED (%d)\n", argv[0], ret);
    }

    return ret;
}
