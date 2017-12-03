#define LOG_TAG "EaselTimerTest"

#include "gtest/gtest.h"
#include <log/log.h>

#include "EaselTimer.h"

std::atomic_bool gFlag;
std::atomic_int gCounter;

static void assertFlag()
{
    gFlag = true;
}

static void incrementCounter()
{
    gCounter++;
}

TEST(EaselTimerTest, StopBeforeStart) {
    EaselTimer timer;

    ASSERT_EQ(timer.stop(), -ENODEV);
}

TEST(EaselTimerTest, RestartBeforeStart) {
    EaselTimer timer;

    ASSERT_EQ(timer.restart(), -ENODEV);
}

TEST(EaselTimerTest, StartBeforeStart) {
    EaselTimer timer;

    gFlag = false;
    ASSERT_EQ(timer.start(std::chrono::milliseconds(1000), assertFlag), 0);
    ASSERT_EQ(timer.start(std::chrono::milliseconds(1000), assertFlag), -EBUSY);
    ASSERT_EQ(timer.stop(), 0);
    ASSERT_EQ(gFlag, false);
}

TEST(EaselTimerTest, FireOnce) {
    EaselTimer timer;

    gCounter = 0;
    ASSERT_EQ(timer.start(std::chrono::milliseconds(10), incrementCounter, /*fireOnce=*/true), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(timer.stop(), 0);
    ASSERT_EQ(gCounter, 1);
}

TEST(EaselTimerTest, Periodic) {
    EaselTimer timer;

    gCounter = 0;
    ASSERT_EQ(timer.start(std::chrono::milliseconds(10), incrementCounter), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(timer.stop(), 0);
    // Because of thread scheduling, this may not be exactly 10.
    ASSERT_NEAR(gCounter, 8, 2);
}

TEST(EaselTimerTest, StopBeforeFire) {
    EaselTimer timer;

    gFlag = false;
    ASSERT_EQ(timer.start(std::chrono::milliseconds(100), assertFlag), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_EQ(timer.stop(), 0);
    ASSERT_EQ(gFlag, false);
}

TEST(EaselTimerTest, RestartBeforeFire) {
    EaselTimer timer;

    gFlag = false;
    ASSERT_EQ(timer.start(std::chrono::milliseconds(100), assertFlag), 0);
    for (int i = 0; i < 10; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ASSERT_EQ(timer.restart(), 0);
    }
    ASSERT_EQ(timer.stop(), 0);
    ASSERT_EQ(gFlag, false);
}

TEST(EaselTimerTest, RestartAfterStop) {
    EaselTimer timer;

    gFlag = false;
    ASSERT_EQ(timer.start(std::chrono::milliseconds(100), assertFlag), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_EQ(timer.stop(), 0);
    ASSERT_EQ(timer.restart(), -ENODEV);
    ASSERT_EQ(gFlag, false);
}

TEST(EaselTimerTest, TwoTimers) {
    EaselTimer timer1, timer2;
    auto fn1 = [&timer2]() { ASSERT_EQ(timer2.restart(), 0); };
    auto fn2 = []() { ASSERT_TRUE(false); };

    ASSERT_EQ(timer1.start(std::chrono::milliseconds(100), fn1), 0);
    ASSERT_EQ(timer2.start(std::chrono::milliseconds(200), fn2, /*fireOnce=*/true), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    ASSERT_EQ(timer1.stop(), 0);
    ASSERT_EQ(timer2.stop(), 0);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
