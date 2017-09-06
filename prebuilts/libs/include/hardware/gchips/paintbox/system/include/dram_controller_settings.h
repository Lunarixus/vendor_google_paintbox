#ifndef HARDWARE_GCHIPS_PAINTBOX_SYSTEM_INCLUDE_DRAM_CONTROLLER_SETTINGS_H_
#define HARDWARE_GCHIPS_PAINTBOX_SYSTEM_INCLUDE_DRAM_CONTROLLER_SETTINGS_H_

// Functions to set Paintbox (Easel) DRAM Controller registers - this is to
// tune the DRAM Controller for different traffic patterns (based on
// application modes such as continuous MIPI capture). Current supported
// settings are "Default" and "ContinuousCapture".
// Usage:
//   For MIPI continuous capture:
//     paintbox::dram_controller::ContinuousCaptureSettings();
//   Default settings (for faster HDR+ if there is no concurrent MIPI traffic):
//     paintbox::dram_controller::DefaultSettings();
//
// Note on thread safety: These functions are not protected by locks.
// It is the application's responsibility to set up appropriate locking
// mechanisms to protect against simultaneous reads/writes of the registers.

namespace paintbox {
namespace dram_controller {

// Sets Paintbox DRAM Controller AXI register settings to default mode.
void DefaultSettings();
// Sets Paintbox DRAM Controller AXI register settings to support continuous
// capture mode.
void ContinuousCaptureSettings();

}  // namespace dram_controller
}  // namespace paintbox

#endif  // HARDWARE_GCHIPS_PAINTBOX_SYSTEM_INCLUDE_DRAM_CONTROLLER_SETTINGS_H_
