#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "easelmipi_service.h"
#include "uapi/linux/mipibridge.h"

static const char* kEaselMipiDev = "/dev/mipi";

static mipicsi_top_dev ConvertMipiRxPort(MipiRxPort rx) {
    switch(rx) {
        case RX_0:
            return MIPI_RX0;
        case RX_1:
            return MIPI_RX1;
        case RX_2:
            return MIPI_RX2;
        default:
            return MIPI_RX0;
    }
}

static mipicsi_top_dev ConvertMipiTxPort(MipiTxPort tx) {
    switch(tx) {
        case TX_0:
            return MIPI_TX0;
        case TX_1:
            return MIPI_TX1;
        default:
            return MIPI_TX0;
    }
}

// TODO(cjluo): properly log errno for debugging

int EaselMipiService::init() {
    mEaselMipiFd = open(kEaselMipiDev, O_RDWR);
    if (mEaselMipiFd == -1) {
        return -1;
    }
    return 0;
}

void EaselMipiService::release() {
    close(mEaselMipiFd);
}

int EaselMipiService::enableRx(MipiRxPort rx, uint32_t num_lanes, uint32_t bitrate_mbps) {
    struct csi2_vc_dt_pair deprecated;
    struct mipicsi_top_cfg config = {ConvertMipiRxPort(rx), num_lanes, bitrate_mbps,
            {deprecated} /* deprecated field */};

    return ioctl(mEaselMipiFd, MIPI_TOP_START, &config);
}

int EaselMipiService::enableTx(MipiTxPort tx, uint32_t num_lanes, uint32_t bitrate_mbps) {
    struct csi2_vc_dt_pair deprecated;
    struct mipicsi_top_cfg config = {ConvertMipiTxPort(tx), num_lanes, bitrate_mbps,
            {deprecated} /* deprecated field */};

    return ioctl(mEaselMipiFd, MIPI_TOP_START, &config);
}

int EaselMipiService::disableRx(MipiRxPort rx) {
    return ioctl(mEaselMipiFd, MIPI_TOP_STOP, ConvertMipiRxPort(rx));
}

int EaselMipiService::disableTx(MipiTxPort tx) {
    return ioctl(mEaselMipiFd, MIPI_TOP_STOP, ConvertMipiTxPort(tx));
}

int EaselMipiService::setBypass(MipiRxPort rx, MipiTxPort tx, bool /* to be used */) {
    struct mipicsi_top_mux config = {ConvertMipiRxPort(rx), ConvertMipiTxPort(tx), false};

    if (ioctl(mEaselMipiFd, MIPI_TOP_G_MUX, &config) == -1) {
        return -1;
    }

    // MUX is already active.
    if (config.active) {
        return 0;
    }

    config.active = true;
    return ioctl(mEaselMipiFd, MIPI_TOP_S_MUX, &config);
}

int EaselMipiService::setFunctional(MipiTxPort tx, bool /* to be used */) {
    struct mipicsi_top_mux config = {MIPI_IPU, ConvertMipiTxPort(tx), false};
    if (ioctl(mEaselMipiFd, MIPI_TOP_G_MUX, &config) == -1) {
        return -1;
    }

    // MUX is already active.
    if (config.active) {
        return 0;
    }

    config.active = true;
    return ioctl(mEaselMipiFd, MIPI_TOP_S_MUX, &config);
}

int EaselMipiService::getBypassStatus(MipiRxPort rx, MipiTxPort tx, MipiMuxStatus* status) {
    struct mipicsi_top_mux config = {ConvertMipiRxPort(rx), ConvertMipiTxPort(tx), false};

    if (ioctl(mEaselMipiFd, MIPI_TOP_G_MUX, &config) == -1) {
        return -1;
    }
    status->active = config.active;
    return 0;
}

int EaselMipiService::getFunctionalStatus(MipiTxPort tx, MipiMuxStatus* status) {
    struct mipicsi_top_mux config = {MIPI_IPU, ConvertMipiTxPort(tx), false};

    if (ioctl(mEaselMipiFd, MIPI_TOP_G_MUX, &config) == -1) {
        return -1;
    }
    status->active = config.active;
    return 0;
}

int EaselMipiService::reset() {
    // Not implemented
    return 0;
}