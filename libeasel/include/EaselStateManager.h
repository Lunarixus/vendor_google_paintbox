#ifndef __EASEL_STATE_MANAGER_H__
#define __EASEL_STATE_MANAGER_H__

#include <stdio.h>
#include <unistd.h>

class EaselStateManager {

public:
    enum State {
        ESM_STATE_OFF, // powered off
        ESM_STATE_INIT, // powered on, unconfigured
        ESM_STATE_CONFIG_MIPI, // powered on, mipi configured
        ESM_STATE_CONFIG_DDR, // powered on, ddr configured
        ESM_STATE_ACTIVE, // powered on and booted
        ESM_STATE_SUSPEND_SELF_REFRESH, // suspended, ddr in self-refresh
        ESM_STATE_SUSPEND_HIBERNATE, // suspended, kernel image in AP DRAM
        ESM_STATE_MAX,
    };

    struct EaselMipiConfig {
        enum {
            ESL_MIPI_RX_CHAN_0,
            ESL_MIPI_RX_CHAN_1,
            ESL_MIPI_RX_CHAN_2,
        } rxChannel;

        enum {
            ESL_MIPI_TX_CHAN_0,
            ESL_MIPI_TX_CHAN_1,
        } txChannel;

        int rxRate;
        int txRate;
    };

    EaselStateManager(): mFd(-1) {};
    ~EaselStateManager() {
        if (mFd >= 0)
            close(mFd);
    }

    int init();

    int powerOn(bool blocking = true);
    int powerOff(bool blocking = true);
    int configMipi(EaselMipiConfig *config);
    int configDdr(bool blocking = true);
    int download(bool blocking = true);
    int getState(enum State *state);
    int setState(enum State state, bool blocking = true);

private:
    int mFd;

    int waitForState(enum State state);
};

#endif // __EASEL_STATE_MANAGER_H__
