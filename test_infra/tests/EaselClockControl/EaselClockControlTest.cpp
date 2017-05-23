/*
 * EaselClockControl unit tests
 */

#define LOG_TAG "EaselClockControlTest"

#include <log/log.h>

#include "EaselClockControl.h"
#include "gtest/gtest.h"

TEST(EaselClockControlTest, Sys200Apis) {
    int ret;
    bool enable;

    // Test sys200 mode
    ret = EaselClockControl::setSys200Mode();
    ASSERT_EQ(ret, 0);

    ret = EaselClockControl::getSys200Mode(&enable);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(enable, 1);

    ret = EaselClockControl::setFrequency(EaselClockControl::Subsystem::CPU, 950);
    ASSERT_EQ(ret, 0);

    ret = EaselClockControl::getSys200Mode(&enable);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(enable, 0);
}

TEST(EaselClockControlTest, CpuApis) {
    int ret;
    int freq;

    ret = EaselClockControl::setFrequency(EaselClockControl::Subsystem::CPU, 950);
    ASSERT_EQ(ret, 0);

    freq = EaselClockControl::getFrequency(EaselClockControl::Subsystem::CPU);
    ASSERT_EQ(freq, 950);

    ret = EaselClockControl::setFrequency(EaselClockControl::Subsystem::CPU, 200);
    ASSERT_EQ(ret, 0);

    freq = EaselClockControl::getFrequency(EaselClockControl::Subsystem::CPU);
    ASSERT_EQ(freq, 200);

    ret = EaselClockControl::setFrequency(EaselClockControl::Subsystem::CPU, 400);
    ASSERT_EQ(ret, 0);

    freq = EaselClockControl::getFrequency(EaselClockControl::Subsystem::CPU);
    ASSERT_EQ(freq, 400);

    ret = EaselClockControl::setFrequency(EaselClockControl::Subsystem::CPU, 500);
    ASSERT_EQ(ret, 0);

    freq = EaselClockControl::getFrequency(EaselClockControl::Subsystem::CPU);
    ASSERT_EQ(freq, 600);
}


TEST(EaselClockControlTest, IpuApis) {
    int ret;
    int freq;

    ret = EaselClockControl::setFrequency(EaselClockControl::Subsystem::IPU, 425);
    ASSERT_EQ(ret, 0);

    freq = EaselClockControl::getFrequency(EaselClockControl::Subsystem::IPU);
    ASSERT_EQ(freq, 425);

    ret = EaselClockControl::setFrequency(EaselClockControl::Subsystem::IPU, 100);
    ASSERT_EQ(ret, 0);

    freq = EaselClockControl::getFrequency(EaselClockControl::Subsystem::IPU);
    ASSERT_EQ(freq, 100);

    ret = EaselClockControl::setFrequency(EaselClockControl::Subsystem::IPU, 200);
    ASSERT_EQ(ret, 0);

    freq = EaselClockControl::getFrequency(EaselClockControl::Subsystem::IPU);
    ASSERT_EQ(freq, 200);

    ret = EaselClockControl::setFrequency(EaselClockControl::Subsystem::IPU, 250);
    ASSERT_EQ(ret, 0);

    freq = EaselClockControl::getFrequency(EaselClockControl::Subsystem::IPU);
    ASSERT_EQ(freq, 300);
}

TEST(EaselClockControlTest, LpddrApis) {
    int ret;
    int freq;

    ret = EaselClockControl::setFrequency(EaselClockControl::Subsystem::LPDDR, 2400);
    ASSERT_EQ(ret, 0);

    freq = EaselClockControl::getFrequency(EaselClockControl::Subsystem::LPDDR);
    ASSERT_EQ(freq, 2400);

    ret = EaselClockControl::setFrequency(EaselClockControl::Subsystem::LPDDR, 1200);
    ASSERT_EQ(ret, 0);

    freq = EaselClockControl::getFrequency(EaselClockControl::Subsystem::LPDDR);
    ASSERT_EQ(freq, 1200);

    ret = EaselClockControl::setFrequency(EaselClockControl::Subsystem::LPDDR, 400);
    ASSERT_EQ(ret, -EINVAL);

    freq = EaselClockControl::getFrequency(EaselClockControl::Subsystem::LPDDR);
    ASSERT_EQ(freq, 1200);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
