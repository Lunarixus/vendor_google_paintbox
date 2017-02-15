#ifndef __MIDDLEWARE_INCLUDE_MIPI_MUX_H_
#define __MIDDLEWARE_INCLUDE_MIPI_MUX_H_

#include <stdint.h>

// Definition of ports of Easel MIPI RX
enum class MipiRxPort {
  RX0 = 0,
  RX1,
  RX2,
  IPU,
};

// Definition of ports of Easel MIPI TX
enum class MipiTxPort {
  TX0 = 0,
  TX1,
  IPU,
};

// Definition of MIPI MUX state
typedef struct {
  bool active; // Whether the mux of a route is active or not.
} MipiMuxStatus;

// MipiMux is the class to manipulate MIPI MUX on Easel.
class MipiMux {
 public:
  MipiMux();
  ~MipiMux();
  int Enable(MipiRxPort rx, uint32_t num_lanes, uint32_t bitrate_mbps);
  int Enable(MipiTxPort tx, uint32_t num_lanes, uint32_t bitrate_mbps);
  int Disable(MipiRxPort rx);
  int Disable(MipiTxPort tx);
  // Before calling this function, rx (excluding IPU) should be streaming.
  // When force_on is disabled, the mux takes effect on the next vsync high
  // from rx (excluding IPU), otherwise it will switch immediately in the
  // middle of a frame.
  // When force_off is disabled, the mux waits for vsync low before changing
  // state, otherwise it will switch off from original path immediately in
  // the middle of a frame.
  int SetMux(MipiRxPort rx, MipiTxPort tx, bool force_on=false,
             bool force_off=false);
  // It is recommended to explicitly call DisableMux on old path before set
  // a new mux path on shared tx (excluding ipu) to avoid going to unknown
  // states.
  int DisableMux(MipiRxPort rx, MipiTxPort tx, bool force_off=false);
  // Notice mux status will not change until rx starts streaming (excluding
  // IPU).
  int GetMuxStatus(MipiRxPort rx, MipiTxPort tx, MipiMuxStatus* status);
  int Reset(MipiRxPort rx);
  int Reset(MipiTxPort tx);
  int ResetAll();

 private:
  int fd_; // fd of ioctl dev node.
};

#endif  // __HOSTSW_FPGA_MIPI_MUX_H_
