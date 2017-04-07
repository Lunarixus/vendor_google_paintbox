/*
 * EaselStateManager unit tests
 */

#define LOG_TAG "EaselStateManagerTest"

#include <utils/Log.h>

#include "EaselStateManager.h"
#include "gtest/gtest.h"

TEST(EaselStateManagerTest, StateTransitions) {
    EaselStateManager mgr;
    struct EaselStateManager::EaselMipiConfig mainCamConfig = {
        .rxChannel = EaselStateManager::EaselMipiConfig::ESL_MIPI_RX_CHAN_0,
        .txChannel = EaselStateManager::EaselMipiConfig::ESL_MIPI_TX_CHAN_0,
        .rxRate = 1296,
        .txRate = 1296,
    };
    struct EaselStateManager::EaselMipiConfig frontCamConfig = {
        .rxChannel = EaselStateManager::EaselMipiConfig::ESL_MIPI_RX_CHAN_1,
        .txChannel = EaselStateManager::EaselMipiConfig::ESL_MIPI_TX_CHAN_1,
        .rxRate = 648,
        .txRate = 1296,
    };
    enum EaselStateManager::State state;
    int ret;

    ret = mgr.open();
    ALOGI("mgr.open() = %d\n", ret);
    ASSERT_EQ(ret, 0);

    ret = mgr.setState(EaselStateManager::ESM_STATE_ACTIVE);
    ALOGI("mgr.setState(PENDING) = %d\n", ret);
    EXPECT_EQ(ret, 0);

    ret = mgr.getState(&state);
    ALOGI("mgr.getState() = %d (ret %d)\n", state, ret);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state, EaselStateManager::ESM_STATE_ACTIVE);

    ret = mgr.startMipi(&mainCamConfig);
    ALOGI("mgr.startMipi(main_cam) = %d\n", ret);
    EXPECT_EQ(ret, 0);

    ret = mgr.startMipi(&frontCamConfig);
    ALOGI("mgr.startMipi(front_cam) = %d\n", ret);
    EXPECT_EQ(ret, 0);

    ret = mgr.stopMipi(&mainCamConfig);
    ALOGI("mgr.stopMipi(main_cam) = %d\n", ret);
    EXPECT_EQ(ret, 0);

    ret = mgr.stopMipi(&frontCamConfig);
    ALOGI("mgr.stopMipi(front_cam) = %d\n", ret);
    EXPECT_EQ(ret, 0);

    ret = mgr.setState(EaselStateManager::ESM_STATE_OFF);
    ALOGI("mgr.setState(OFF) = %d\n", ret);
    EXPECT_EQ(ret, 0);

    ret = mgr.getState(&state);
    ALOGI("mgr.getState() = %d (ret %d)\n", state, ret);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state, EaselStateManager::ESM_STATE_OFF);

    ret = mgr.close();
    ALOGI("mgr.close() ret %d\n", ret);
    EXPECT_EQ(ret, 0);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
