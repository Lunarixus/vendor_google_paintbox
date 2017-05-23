#define LOG_TAG "ThermalZoneTest"

#include "gtest/gtest.h"
#include <log/log.h>

#include "ThermalZone.h"

TEST(ThermalZoneTest, GetTemp) {
    ThermalZone batteryTz((char*)"battery", 1);
    ThermalZone pm8998Tz((char*)"pm8998_tz", 1);
    ThermalZone pmi8998Tz((char*)"pmi8998_tz", 1);
    ThermalZone pm8005Tz((char*)"pm8005_tz", 1);
    ThermalZone bcm15602Tz((char*)"bcm15602_tz", 1);
    int temperature;

    ASSERT_EQ(batteryTz.open(), 0);
    temperature = batteryTz.getTemp();
    EXPECT_GT(temperature, 0);
    EXPECT_LT(temperature, 100000);

    ASSERT_EQ(pm8998Tz.open(), 0);
    temperature = pm8998Tz.getTemp();
    ASSERT_GT(temperature, 0);
    ASSERT_LT(temperature, 100000);

    ASSERT_EQ(pmi8998Tz.open(), 0);
    temperature = pmi8998Tz.getTemp();
    ASSERT_GT(temperature, 0);
    ASSERT_LT(temperature, 100000);

    ASSERT_EQ(pm8005Tz.open(), 0);
    temperature = pm8005Tz.getTemp();
    ASSERT_GT(temperature, 0);
    ASSERT_LT(temperature, 100000);

    ASSERT_EQ(bcm15602Tz.open(), 0);
    temperature = bcm15602Tz.getTemp();
    ASSERT_GT(temperature, 0);
    ASSERT_LT(temperature, 100000);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
