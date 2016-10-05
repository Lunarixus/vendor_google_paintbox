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
}

// Definition of ports of Easel MIPI TX
enum MipiTxPort {
    TX_0,
    TX_1,
}

// Definition of MIPI MUX state
typedef struct {
    bool active // Whether the mux of a route is active or not.
} MipiMuxStatus;

class EaselMipiService {
public:
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
    int enableRx(MipiRxPort rx, uint32_t num_lanes, uint32_t bitrate_mbps);

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
    int enableTx(MipiTxPort tx, uint32_t num_lanes, uint32_t bitrate_mbps);

    /*
     * Disables a particular MIPI rx port and it goes to low power state.
     *
     * rx the rx port to be disabled.
     *
     * Returns zero for success or -1 for failure.
     */
    int disableRx(MipiRxPort rx);

    /*
     * Disables a particular MIPI tx port and it goes to low power state.
     *
     * tx the tx port to be disabled.
     *
     * Returns zero for success or -1 for failure.
     */
    int disableTx(MipiTxPort tx);

    /*
     * Sets the MIPI bypass mux to route from rx to tx. If force is set,
     * the new configuration will be effective immediately, otherwise it
     * will take effect after vsync received on the original rx port.
     *
     * rx the rx port to be routed from.
     * tx the tx port to be routed to.
     * force force set the mux without waiting for vsync .
     *
     * Returns zero for success or -1 for failure.
     */
    int setBypass(MipiRxPort rx, MipiTxPort tx, bool force);

    /*
     * Sets Easel MIPI out port to be routed to IPU MIPI out. Tx0 will
     * be connected to IPU MPO0 if tx is TX_0. Tx1 will be connected to
     * IPU MPO1 if tx is TX_1. If force is set, the new configuration
     * will be effective immediately, otherwise it will take effect after
     * vsync received on the original rx port previously routed to
     * current tx port.
     *
     * tx the tx port to be routed to.
     * force force set the mux without waiting for vsync .
     *
     * Returns zero for success or -1 for failure.
     */
    int setFunctional(MipiTxPort tx, bool force);

    /*
     * Get the current connection status from MIPI rx port to MIPI tx port.
     * Status will be reported through MipiMuxStatus parameter.
     *
     * rx the rx port to be routed from.
     * tx the tx port to be routed to.
     * status reported connection status.
     *
     * Returns zero for success or -1 for failure.
     */
    int getBypassStatus(MipiRxPort rx, MipiTxPort tx, MipiMuxStatus* status);

    /*
     * Get the current connection status from IPU MIPI out port to MIPI tx port.
     * Status will be reported through MipiMuxStatus parameter.
     *
     * rx the rx port to be routed from.
     * tx the tx port to be routed to.
     * status reported connection status.
     *
     * Returns zero for success or -1 for failure.
     */
    int getFunctionalStatus(MipiTxPort tx, MipiMuxStatus* status);

    /*
     * Resets Easel MIPI PHY and Controller to default state.
     *
     * Returns zero for success or -1 for failure.
     */
    int reset();

private:
    int mEaselMipiFd; // fd of ioctl dev node.
};
#endif // PAINTBOX_EASELMIPI_SERVICE_H
