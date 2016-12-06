#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "easelmipi_service.h"
#include "uapi/linux/mipibridge.h"

static const char* kEaselMipiDev = "/dev/mipi";
static const uint8_t kSafeSwitchOnMask = 0xF;
static const uint8_t kSafeSwitchOffMask = 0x0;

static mipicsi_top_dev ConvertMipiRxPort(MipiRxPort rx) {
    switch(rx) {
        case RX_0:
            return MIPI_RX0;
        case RX_1:
            return MIPI_RX1;
        case RX_2:
            return MIPI_RX2;
        case RX_IPU:
            return MIPI_IPU;
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
        case TX_IPU:
            return MIPI_IPU;
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

int EaselMipiService::enable(MipiRxPort rx, uint32_t num_lanes, uint32_t bitrate_mbps) {
    struct mipicsi_top_cfg config = {ConvertMipiRxPort(rx), num_lanes, bitrate_mbps};
    return ioctl(mEaselMipiFd, MIPI_TOP_START, &config);
}

int EaselMipiService::enable(MipiTxPort tx, uint32_t num_lanes, uint32_t bitrate_mbps) {
    struct mipicsi_top_cfg config = {ConvertMipiTxPort(tx), num_lanes, bitrate_mbps};
    return ioctl(mEaselMipiFd, MIPI_TOP_START, &config);
}

int EaselMipiService::disable(MipiRxPort rx) {
    return ioctl(mEaselMipiFd, MIPI_TOP_STOP, ConvertMipiRxPort(rx));
}

int EaselMipiService::disable(MipiTxPort tx) {
    return ioctl(mEaselMipiFd, MIPI_TOP_STOP, ConvertMipiTxPort(tx));
}

int EaselMipiService::setMux(MipiRxPort rx, MipiTxPort tx, bool force_on, bool force_off) {
    assert(!(rx == RX_IPU && tx == TX_IPU));

    struct mipicsi_top_mux config = {
        ConvertMipiRxPort(rx),
        ConvertMipiTxPort(tx),
        force_on ? kSafeSwitchOffMask : kSafeSwitchOnMask,
        !force_off,
        false};

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

int EaselMipiService::disableMux(MipiRxPort rx, MipiTxPort tx, bool force_off) {
    assert(!(rx == RX_IPU and tx == TX_IPU));

    struct mipicsi_top_mux config = {
        ConvertMipiRxPort(rx),
        ConvertMipiTxPort(tx),
        0,
        !force_off,
        false};

    if (ioctl(mEaselMipiFd, MIPI_TOP_G_MUX_STATUS, &config) == -1) {
        return -1;
    }

    // MUX is already inactive.
    if (!config.active) {
        return 0;
    }

    return ioctl(mEaselMipiFd, MIPI_TOP_DIS_MUX, &config);
}

int EaselMipiService::getMuxStatus(MipiRxPort rx, MipiTxPort tx, MipiMuxStatus* status) {
    assert(!(rx == RX_IPU and tx == TX_IPU));

    struct mipicsi_top_mux config = {
        ConvertMipiRxPort(rx),
        ConvertMipiTxPort(tx),
        0  /* not used */,
        false  /* not used */,
        false};

    if (ioctl(mEaselMipiFd, MIPI_TOP_G_MUX_STATUS, &config) == -1) {
        return -1;
    }
    status->active = config.active;
    return 0;
}

int EaselMipiService::reset(MipiRxPort rx) {
    assert(rx != RX_IPU);
    return ioctl(mEaselMipiFd, MIPI_TOP_RESET, ConvertMipiRxPort(rx));
}

int EaselMipiService::reset(MipiTxPort tx) {
    assert(tx != TX_IPU);
    return ioctl(mEaselMipiFd, MIPI_TOP_RESET, ConvertMipiTxPort(tx));
}

int EaselMipiService::resetAll() {
    return ioctl(mEaselMipiFd, MIPI_TOP_RESET_ALL);
}