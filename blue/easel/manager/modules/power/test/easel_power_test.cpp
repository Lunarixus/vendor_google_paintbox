#define LOG_TAG "easel_power_test"

#include <string>

#include "android-base/logging.h"
#include "gtest/gtest.h"
#include "EaselPowerBlue.h"

using android::EaselPowerBlue::EaselPowerBlue;

const unsigned int kDelaySeconds = 3;
const unsigned int kShortDelaySeconds = 1;

void do_delay(const unsigned int seconds) {
  if (seconds > 0) {
    fprintf(stdout,
            "*** %s: intentionally delaying %u seconds, please wait\n",
            LOG_TAG,
            seconds);
    fflush(stdout);
    sleep(seconds);
  }
}

// Tests open(), getFwVersion() and close()
TEST(EaselPowerTest, OpenVersionOffClose) {
  EaselPowerBlue ePower;

  EXPECT_EQ(0, ePower.open());
  EXPECT_EQ(false, ePower.getFwVersion().empty());
  EXPECT_EQ(0, ePower.powerOff());
  ePower.close();
}

// Tests powering on Easel then powering off immediately
TEST(EaselPowerTest, OpenOnOffClose) {
  EaselPowerBlue ePower;

  EXPECT_EQ(0, ePower.open());
  EXPECT_EQ(0, ePower.powerOn());
  EXPECT_EQ(0, ePower.powerOff());
  ePower.close();
}

// Tests powering on Easel then powering off after delay
TEST(EaselPowerTest, OpenOnDelayOffClose) {
  EaselPowerBlue ePower;

  EXPECT_EQ(0, ePower.open());
  EXPECT_EQ(0, ePower.powerOn());
  do_delay(kDelaySeconds);
  EXPECT_EQ(0, ePower.powerOff());
  ePower.close();
}

// Tests suspending Easel immediately then resuming once
TEST(EaselPowerTest, OpenOnSuspendResumeOffClose) {
  EaselPowerBlue ePower;

  EXPECT_EQ(0, ePower.open());
  EXPECT_EQ(0, ePower.powerOn());
  EXPECT_EQ(0, ePower.suspend());
  EXPECT_EQ(0, ePower.resume());
  EXPECT_EQ(0, ePower.powerOff());
  ePower.close();
}

// Tests suspending Easel after delay then resuming once
TEST(EaselPowerTest, OpenOnDelaySuspendResumeOffClose) {
  EaselPowerBlue ePower;

  EXPECT_EQ(0, ePower.open());
  EXPECT_EQ(0, ePower.powerOn());
  do_delay(kDelaySeconds);
  EXPECT_EQ(0, ePower.suspend());
  EXPECT_EQ(0, ePower.resume());
  EXPECT_EQ(0, ePower.powerOff());
  ePower.close();
}

// Tests suspending/resuming Easel for 3 times, with shorter delay
TEST(EaselPowerTest, SuspendResumeTimes3) {
  EaselPowerBlue ePower;

  EXPECT_EQ(0, ePower.open());
  EXPECT_EQ(0, ePower.powerOn());
  do_delay(kDelaySeconds);
  EXPECT_EQ(0, ePower.suspend());
  EXPECT_EQ(0, ePower.resume());
  do_delay(kShortDelaySeconds);
  EXPECT_EQ(0, ePower.suspend());
  EXPECT_EQ(0, ePower.resume());
  do_delay(kShortDelaySeconds);
  EXPECT_EQ(0, ePower.suspend());
  EXPECT_EQ(0, ePower.resume());
  EXPECT_EQ(0, ePower.powerOff());
  ePower.close();
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
