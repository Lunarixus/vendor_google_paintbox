#define LOG_TAG "EaselStateManagerTest"

#include <utils/Log.h>

#include "EaselStateManager.h"

int main()
{
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
    ALOG_ASSERT(ret == 0);

    ret = mgr.setState(EaselStateManager::ESM_STATE_PENDING);
    ALOGI("mgr.setState(PENDING) = %d\n", ret);
    ALOG_ASSERT(ret == 0);

    ret = mgr.getState(&state);
    ALOGI("mgr.getState() = %d (ret %d)\n", state, ret);
    ALOG_ASSERT((ret == 0) && (state == EaselStateManager::ESM_STATE_PENDING));

    ret = mgr.startMipi(&mainCamConfig);
    ALOGI("mgr.startMipi(main_cam) = %d\n", ret);
    ALOG_ASSERT(ret == 0);

    ret = mgr.startMipi(&frontCamConfig);
    ALOGI("mgr.startMipi(front_cam) = %d\n", ret);
    ALOG_ASSERT(ret == 0);

    ret = mgr.setState(EaselStateManager::ESM_STATE_ACTIVE);
    ALOGI("mgr.setState(ACTIVE) = %d\n", ret);
    ALOG_ASSERT(ret == 0);

    ret = mgr.getState(&state);
    ALOGI("mgr.getState() = %d (ret %d)\n", state, ret);
    ALOG_ASSERT((ret == 0) && (state == EaselStateManager::ESM_STATE_ACTIVE));

    ret = mgr.setState(EaselStateManager::ESM_STATE_SUSPEND);
    ALOGI("mgr.setState(SUSPEND) = %d\n", ret);
    ALOG_ASSERT(ret == 0);

    ret = mgr.getState(&state);
    ALOGI("mgr.getState() = %d (ret %d)\n", state, ret);
    ALOG_ASSERT((ret == 0) && (state == EaselStateManager::ESM_STATE_SUSPEND));

    ret = mgr.setState(EaselStateManager::ESM_STATE_PENDING);
    ALOGI("mgr.setState(PENDING) = %d\n", ret);
    ALOG_ASSERT(ret == 0);

    ret = mgr.getState(&state);
    ALOGI("mgr.getState() = %d (ret %d)\n", state, ret);
    ALOG_ASSERT((ret == 0) && (state == EaselStateManager::ESM_STATE_PENDING));

    ret = mgr.setState(EaselStateManager::ESM_STATE_ACTIVE);
    ALOGI("mgr.setState(ACTIVE) = %d\n", ret);
    ALOG_ASSERT(ret == 0);

    ret = mgr.getState(&state);
    ALOGI("mgr.getState() = %d (ret %d)\n", state, ret);
    ALOG_ASSERT((ret == 0) && (state == EaselStateManager::ESM_STATE_ACTIVE));

    ret = mgr.setState(EaselStateManager::ESM_STATE_OFF);
    ALOGI("mgr.setState(OFF) = %d\n", ret);
    ALOG_ASSERT(ret == 0);

    ret = mgr.getState(&state);
    ALOGI("mgr.getState() = %d (ret %d)\n", state, ret);
    ALOG_ASSERT((ret == 0) && (state == EaselStateManager::ESM_STATE_OFF));

    return 0;
}
