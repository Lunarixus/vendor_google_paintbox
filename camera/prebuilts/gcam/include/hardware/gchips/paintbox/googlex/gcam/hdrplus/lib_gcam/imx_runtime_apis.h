#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_IMX_RUNTIME_APIS_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_IMX_RUNTIME_APIS_H_

// This file is provided for external apps (such as pbserver) to access
// IPU/IMX Runtime APIs via gcam.

namespace gcam {

// Provides an interface to the halide_ipu_load_precompiled_graphs Halide IPU
// Runtime API
extern void LoadPrecompiledGraphs();

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_IMX_RUNTIME_APIS_H_
