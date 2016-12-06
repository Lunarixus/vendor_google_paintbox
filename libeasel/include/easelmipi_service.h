#ifndef PAINTBOX_EASELMIPI_SERVICE_H
#define PAINTBOX_EASELMIPI_SERVICE_H

/*
 * EaselMipiService is the service API on Easel to control MIPI MUX.
 */

// Definition of ports of Easel MIPI RX
enum MipiRxPort {
    RX_0,
    RX_1,
    RX_2,
    RX_IPU,
};

// Definition of ports of Easel MIPI TX
enum MipiTxPort {
    TX_0,
    TX_1,
    TX_IPU,
};

// Definition of MIPI MUX state
typedef struct {
    bool active; // Whether the mux of a route is active or not.
} MipiMuxStatus;

class EaselMipiService {
public:
    EaselMipiService() { init(); }
    ~EaselMipiService() { release(); };
    /*
     * Initializes Easel MIPI service.
     *
     * Returns zero for success or -1 for failure.
     */
    int init();

    /*
     * Releases Easel MIPI services.
     */
    void release();

    /*
     * Configures MIPI DPHY and controller of a particular MIPI rx port.
     * This function has to be called before setting the mux to connect
     * to the port.
     *
     * rx the rx port to be enabled.
     * num_lanes number of lanes.
     * bitrate_mbps bitrate of camera sensor in MBPS per lane.
     *
     * Returns zero for success or -1 for failure.
     */
    int enable(MipiRxPort rx, uint32_t num_lanes, uint32_t bitrate_mbps);

    /*
     * Configures MIPI DPHY and controller of a particular MIPI tx port.
     * This function has to be called before setting the mux to connect
     * to a particular port.
     *
     * tx the tx port to be enabled.
     * num_lanes number of lanes.
     * bitrate_mbps bitrate of tx port in MBPS per lane.
     *
     * Returns zero for success or -1 for failure.
     */
    int enable(MipiTxPort tx, uint32_t num_lanes, uint32_t bitrate_mbps);

    /*
     * Disables a particular MIPI rx port and it goes to low power state.
     *
     * rx the rx port to be disabled.
     *
     * Returns zero for success or -1 for failure.
     */
    int disable(MipiRxPort rx);

    /*
     * Disables a particular MIPI tx port and it goes to low power state.
     *
     * tx the tx port to be disabled.
     *
     * Returns zero for success or -1 for failure.
     */
    int disable(MipiTxPort tx);

    /*
     * Sets the MIPI smart mux to route from rx to tx.
     *
     * Before calling this function, rx (excluding IPU) should be streaming.
     *
     * When force_on is disabled, the mux switches on the next vsync high
     * from rx (excluding IPU), otherwise it will switch immediately in the
     * middle of a frame.
     *
     * When force_off is disabled, the next mux switch call (setMux, disableMux)
     * will wait for vsync low before turning the current route off,
     * otherwise it will be switched off from current route immediately
     * in the middle of a frame.
     *
     * rx the rx port to be routed from, including IPU.
     * tx the tx port to be routed to, including IPU.
     * force_on force set the mux on without waiting for vsync.
     * force_off in the next mux switch call, force set current mux off
     *    without waiting for vsync.
     *
     * Returns zero for success or -1 for failure.
     */
    int setMux(MipiRxPort rx, MipiTxPort tx, bool force_on=false,
            bool force_off=false);

    /*
     * Disables the MIPI smart mux routing from rx to tx.
     *
     * It is recommended to explicitly call disableMux on old path before setting
     * a new mux path on shared tx (excluding ipu) to avoid going to unknown
     * states.
     *
     * When force_off is disabled, the mux waits for vsync low before turning
     * the current route off, otherwise it will disable the current route immediately
     * in the middle of a frame.
     *
     * rx the rx port to be routed from.
     * tx the tx port to be routed to.
     * force_off force set current mux off without waiting for vsync.
     *
     * Returns zero for success or -1 for failure.
     */
    int disableMux(MipiRxPort rx, MipiTxPort tx, bool force_off=false);

    /*
     * Get the current connection status from rx port to tx port.
     * Status will be reported through MipiMuxStatus parameter.
     *
     * rx the rx port to be routed from.
     * tx the tx port to be routed to.
     * status reported connection status.
     *
     * Returns zero for success or -1 for failure.
     */
    int getMuxStatus(MipiRxPort rx, MipiTxPort tx, MipiMuxStatus* status);

    /*
     * Resets Easel MIPI PHY and Controller to default state.
     *
     * rx the rx port to be reset.
     *
     * Returns zero for success or -1 for failure.
     */
    int reset(MipiRxPort rx);

    /*
     * Resets Easel MIPI PHY and Controller to default state.
     *
     * tx the tx port to be reset.
     *
     * Returns zero for success or -1 for failure.
     */
    int reset(MipiTxPort tx);

    /*
     * Resets all Easel MIPI PHYs and Controllers to default state.
     *
     * Returns zero for success or -1 for failure.
     */
    int resetAll();

private:
    int mEaselMipiFd; // fd of ioctl dev node.
};
#endif // PAINTBOX_EASELMIPI_SERVICE_H
