#define LOG_TAG "EaselControlTest"

#include <getopt.h>
#include <time.h>
#include <utils/Log.h>

#include "easelcontrol.h"

namespace android {

int body(int sec, int iterations) {
    int ret;
    EaselControlClient easelControl;

    ALOGI("Start testing Easel Boot for %d cycles, staying %d secs each.\n",
          iterations, sec);

    struct timespec begin;
    struct timespec now;
    int64_t diff_ms;

    for (int i = 1; i < iterations + 1; i++) {
        ret = easelControl.open();
        ALOG_ASSERT(ret == 0);

        ret = easelControl.suspend();
        ALOG_ASSERT(ret == 0);

        // Start timer
        clock_gettime(CLOCK_MONOTONIC, &begin);

        ret = easelControl.resume();
        ALOG_ASSERT(ret == 0);

        ret = easelControl.activate();
        ALOG_ASSERT(ret == 0);

        // Stop timer
        clock_gettime(CLOCK_MONOTONIC, &now);
        diff_ms = (now.tv_sec - begin.tv_sec) * 1000
                  + (now.tv_nsec - begin.tv_nsec) / 1000000;
        ALOGI("iter %d: Easel resume->activate done: %ld ms!\n", i, diff_ms);

        if (sec >= 0) {
            sleep(sec);
        } else {
            getchar();
        }

        ret = easelControl.deactivate();
        ALOG_ASSERT(ret == 0);

        ALOGI("easelControl.suspend()\n");
        easelControl.suspend();

        ALOGI("easelControl.close()\n");
        easelControl.close();

        ALOGI("Testing Easel Boot n. %d done!\n", i);

        if (sec >= 0) {
            sleep(sec);
        } else {
            getchar();
        }
    }
    return 0;
}

}   // namespace android

int main(int argc, char **argv) {
    int ch;
    int numIteration = 1;
    int numSleepSecond = 1;

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

    return android::body(numSleepSecond, numIteration);
}
