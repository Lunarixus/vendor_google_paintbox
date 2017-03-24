#ifndef __EASEL_STATE_MANAGER_H__
#define __EASEL_STATE_MANAGER_H__

#include <stdio.h>
#include <unistd.h>

#include "uapi/linux/mnh-sm.h"

class EaselStateManager {

public:
    enum State {
        ESM_STATE_OFF = MNH_STATE_OFF, // powered off
        ESM_STATE_PENDING = MNH_STATE_PENDING, // powered on, pcie ready, CPU in PBL
        ESM_STATE_ACTIVE = MNH_STATE_ACTIVE, // powered on and booted
        ESM_STATE_SUSPEND = MNH_STATE_SUSPEND, // suspended, ddr in self-refresh
        ESM_STATE_MAX = MNH_STATE_MAX,
    };

    struct EaselMipiConfig {
        enum {
            ESL_MIPI_RX_CHAN_0,
            ESL_MIPI_RX_CHAN_1,
            ESL_MIPI_RX_CHAN_2,
            ESL_MIPI_RX_IPU,
        } rxChannel;

        enum {
            ESL_MIPI_TX_CHAN_0,
            ESL_MIPI_TX_CHAN_1,
            ESL_MIPI_TX_IPU,
        } txChannel;

        enum {
            ESL_MIPI_MODE_BYPASS,
            ESL_MIPI_MODE_BYPASS_W_IPU,
            ESL_MIPI_MODE_FUNCTIONAL,
        } mode;

        int rxRate;
        int txRate;
    };

    EaselStateManager(): mFd(-1) {};

    int open();
    int close();

    /*
     * Starts (or restarts) a MIPI session.
     *
     * config: describes MIPI configuration options.
     *
     * Returns 0 for success; otherwise, returns error number.
     */
    int startMipi(EaselMipiConfig *config);

    /*
     * Stops a MIPI session.
     *
     * config: describes MIPI configuration options.
     *
     * Returns 0 for success; otherwise, returns error number.
     */
    int stopMipi(EaselMipiConfig *config);

    /*
     * Gets the current system state.
     *
     * state: reference to current state, set in method.
     *
     * Returns 0 for success; otherwise, returns error number.
     */
    int getState(enum State *state);

    /*
     * Sets the current system state.
     *
     * state: desired state.
     * blocking: use "true" to wait until state transition has occurred; use
     *           "false" if method should return immediately.
     *
     * Returns 0 for success; otherwise, returns error number.
     */
    int setState(enum State state, bool blocking = true);

    /*
     * Blocks until state is reached.
     *
     * state: desired state.
     *
     * Returns 0 for success; otherwise, returns error number.
     */
    int waitForState(enum State state);

private:
    int mFd;
};

#endif // __EASEL_STATE_MANAGER_H__
