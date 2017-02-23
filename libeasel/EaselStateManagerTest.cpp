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

    ret = mgr.init();
    ALOGI("mgr.init() = %d\n", ret);
    ALOG_ASSERT(ret == 0);

    ret = mgr.powerOn();
    ALOGI("mgr.powerOn() = %d\n", ret);
    ALOG_ASSERT(ret == 0);

    ret = mgr.getState(&state);
    ALOGI("mgr.getState() = %d (ret %d)\n", state, ret);
    ALOG_ASSERT((ret == 0) && (state == EaselStateManager::ESM_STATE_INIT));

    ret = mgr.configMipi(&mainCamConfig);
    ALOGI("mgr.configMipi(main_cam) = %d\n", ret);
    ALOG_ASSERT(ret == 0);

    ret = mgr.getState(&state);
    ALOGI("mgr.getState() = %d (ret %d)\n", state, ret);
    ALOG_ASSERT((ret == 0) && (state == EaselStateManager::ESM_STATE_CONFIG_MIPI));

    ret = mgr.configMipi(&frontCamConfig);
    ALOGI("mgr.configMipi(front_cam) = %d\n", ret);
    ALOG_ASSERT(ret == 0);

    ret = mgr.getState(&state);
    ALOGI("mgr.getState() = %d (ret %d)\n", state, ret);
    ALOG_ASSERT((ret == 0) && (state == EaselStateManager::ESM_STATE_CONFIG_MIPI));

    ret = mgr.configDdr();
    ALOGI("mgr.configDdr() = %d\n", ret);
    ALOG_ASSERT(ret == 0);

    ret = mgr.getState(&state);
    ALOGI("mgr.getState() = %d (ret %d)\n", state, ret);
    ALOG_ASSERT((ret == 0) && (state == EaselStateManager::ESM_STATE_CONFIG_DDR));

    ret = mgr.download();
    ALOGI("mgr.download() = %d\n", ret);
    ALOG_ASSERT(ret == 0);

    ret = mgr.getState(&state);
    ALOGI("mgr.getState() = %d (ret %d)\n", state, ret);
    ALOG_ASSERT((ret == 0) && (state == EaselStateManager::ESM_STATE_ACTIVE));

    ret = mgr.powerOff();
    ALOGI("mgr.powerOff() = %d\n", ret);
    ALOG_ASSERT(ret == 0);

    ret = mgr.getState(&state);
    ALOGI("mgr.getState() = %d (ret %d)\n", state, ret);
    ALOG_ASSERT((ret == 0) && (state == EaselStateManager::ESM_STATE_OFF));

    return 0;
}
