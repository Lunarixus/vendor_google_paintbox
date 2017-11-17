#define LOG_TAG "EaselBootTest"

#include "easelcontrol.h"

#include <cutils/properties.h>
#include <gtest/gtest.h>
#include <log/log.h>

#include <getopt.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define PMIC_SYSFS_FILE_WAHOO      "/sys/devices/soc/c1b7000.i2c/i2c-9/9-0008/toggle_pon"
#define PMIC_SYSFS_FILE_BLUECROSS  "/sys/devices/platform/soc/a88000.i2c/i2c-0/0-0066/toggle_pon"

static int numIteration = 1;
static int numSleepSecond = 1;

TEST(EaselBootTest, PmicPowerCycle) {
    int fd = -1;
    char device[PROPERTY_VALUE_MAX];
    char str[32];

    ALOGI("Start testing Easel Boot for %d cycles, staying %d secs each.",
          numIteration, numSleepSecond);

    for (int i = 0; i < numIteration; i++) {
        property_get("ro.hardware", device, "default");
        if ((strcmp(device, "walleye") == 0) ||
            (strcmp(device, "taimen") == 0)) {
            fd = open(PMIC_SYSFS_FILE_WAHOO, O_RDONLY);
        } else if ((strcmp(device, "blueline") == 0) ||
                   (strcmp(device, "crosshatch") == 0)) {
            fd = open(PMIC_SYSFS_FILE_BLUECROSS, O_RDONLY);
        }
        ASSERT_GE(fd, 0) << "failed to open PMIC sysfs file (" << errno << ").";

        ASSERT_GE(read(fd, str, 32), 0);

        ASSERT_EQ(close(fd), 0) << "failed to close PMIC sysfs file (" << errno << ").";

        if (numSleepSecond >= 0) {
            sleep(numSleepSecond);
        }
    }
}

TEST(EaselBootTest, BootShutdownLoop) {
    EaselControlClient easelControl;

    ALOGI("Start testing Easel Boot for %d cycles, staying %d secs each.",
          numIteration, numSleepSecond);

    for (int i = 0; i < numIteration; i++) {
        ASSERT_EQ(easelControl.open(), 0);

        ASSERT_EQ(easelControl.resume(), 0);

        if (numSleepSecond >= 0) {
            sleep(numSleepSecond);
        }

        ASSERT_EQ(easelControl.suspend(), 0);

        ALOGI("Testing Easel Boot n. %d done", i);

        if (numSleepSecond >= 0) {
            sleep(numSleepSecond);
        }

        easelControl.close();
    }
}

TEST(EaselBootTest, BootShutdownLoopWithActivate) {
    EaselControlClient easelControl;
    struct timespec begin;
    struct timespec now;
    int64_t diff_ms;

    ALOGI("Start testing Easel Boot for %d cycles, staying %d secs each.",
          numIteration, numSleepSecond);

    for (int i = 0; i < numIteration; i++) {
        ASSERT_EQ(easelControl.open(), 0);

        ASSERT_EQ(easelControl.suspend(), 0);

        // Start timer
        clock_gettime(CLOCK_MONOTONIC, &begin);

        ASSERT_EQ(easelControl.resume(), 0);

        ASSERT_EQ(easelControl.activate(), 0);

        // Stop timer
        clock_gettime(CLOCK_MONOTONIC, &now);
        diff_ms = (now.tv_sec - begin.tv_sec) * 1000
                  + (now.tv_nsec - begin.tv_nsec) / 1000000;
        ALOGI("iter %d: Easel resume->activate done: %" PRId64 " ms", i, diff_ms);

        if (numSleepSecond >= 0) {
            sleep(numSleepSecond);
        }

        ASSERT_EQ(easelControl.deactivate(), 0);

        ASSERT_EQ(easelControl.suspend(), 0);

        ALOGI("Testing Easel Boot n. %d done", i);

        if (numSleepSecond >= 0) {
            sleep(numSleepSecond);
        }

        easelControl.close();
    }
}

TEST(EaselBootTest, SuspendResumeLoop) {
    EaselControlClient easelControl;

    ALOGI("Start testing Easel Boot for %d cycles, staying %d secs each.",
          numIteration, numSleepSecond);

    ASSERT_EQ(easelControl.open(), 0);

    for (int i = 0; i < numIteration; i++) {
        ASSERT_EQ(easelControl.resume(), 0);

        if (numSleepSecond >= 0) {
            sleep(numSleepSecond);
        }

        ASSERT_EQ(easelControl.suspend(), 0);

        ALOGI("Testing Easel Boot n. %d done", i);

        if (numSleepSecond >= 0) {
            sleep(numSleepSecond);
        }
    }

    easelControl.close();
}

TEST(EaselBootTest, SuspendResumeLoopWithActivate) {
    EaselControlClient easelControl;
    struct timespec begin;
    struct timespec now;
    int64_t diff_ms;

    ALOGI("Start testing Easel Boot for %d cycles, staying %d secs each.",
          numIteration, numSleepSecond);

    ASSERT_EQ(easelControl.open(), 0);

    for (int i = 0; i < numIteration; i++) {
        ASSERT_EQ(easelControl.suspend(), 0);

        // Start timer
        clock_gettime(CLOCK_MONOTONIC, &begin);

        ASSERT_EQ(easelControl.resume(), 0);

        ASSERT_EQ(easelControl.activate(), 0);

        // Stop timer
        clock_gettime(CLOCK_MONOTONIC, &now);
        diff_ms = (now.tv_sec - begin.tv_sec) * 1000
                  + (now.tv_nsec - begin.tv_nsec) / 1000000;
        ALOGI("iter %d: Easel resume->activate done: %" PRId64 " ms", i, diff_ms);

        if (numSleepSecond >= 0) {
            sleep(numSleepSecond);
        }

        ASSERT_EQ(easelControl.deactivate(), 0);

        ASSERT_EQ(easelControl.suspend(), 0);

        ALOGI("Testing Easel Boot n. %d done", i);

        if (numSleepSecond >= 0) {
            sleep(numSleepSecond);
        }
    }

    easelControl.close();
}

int main(int argc, char **argv) {
    int ch;

    ::testing::InitGoogleTest(&argc, argv);

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
            fprintf(stderr,
                    "Usage: %s -s <sleep_sec> -i <iter>\n", argv[0]);
            fprintf(stderr,
                    "       -h              This help.\n"
                    "       -s <sleep_sec>  Num of seconds. \n"
                    "                       -1 to wait until user input.\n"
                    "       -i <iter>       Num of iterations.\n"
                    );
            exit(1);
            break;
          default:
            break;
        }
    }

    return RUN_ALL_TESTS();
}
