#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_IMX_RUNTIME_APIS_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_IMX_RUNTIME_APIS_H_

// This file is provided for external apps (such as pbserver) to access
// IPU/IMX Runtime APIs via gcam.

namespace gcam {

// Provides an interface to the halide_ipu_load_precompiled_graphs Halide IPU
// Runtime API
extern void LoadPrecompiledGraphs();

// Provides an interface to the halide_ipu_acquire_ipu_hw Halide IPU
// Runtime API.
// Currently, this acquires all IPU resources (STPs, DMA channels, LBPs).
// It is an error to call this function multiple times without releasing IPU
// (with a call to ReleaseIpuDeviceIfAcquired).
// Note that acquiring an IPU device turns on the resources, resulting in
// increased leakage power. Acquiring takes around 100us for Easel.
extern void AcquireIpuDevice();

// Provides an interface to the halide_ipu_release_ipu_hw Halide IPU
// Runtime API.
// Releases a previously acquired IPU device, if any. It is ok to call this
// function without previously having acquired an IPU device (with a call to
// AcquireIpuDevice) - this is is effectively a nop.
// Releasing an IPU device will power off the resources.
extern void ReleaseIpuDeviceIfAcquired();

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_IMX_RUNTIME_APIS_H_
