#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_IPU_ALGORITHMS_PUBLIC_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_IPU_ALGORITHMS_PUBLIC_H_

#include "googlex/gcam/image/yuv.h"

namespace gcam {

// Wrapper of gcam::ResampleIpu in ipu_algorithms.h
// Returns true if YUV downsample is successful, otherwise false.
// TODO(b/65124689): remove copy_to_host after resolving Imx buffer issue in
// pbserver.
bool ResampleIpu(const YuvReadView& src_map, YuvWriteView* dst_map,
                 bool copy_to_host = false);

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_IPU_ALGORITHMS_PUBLIC_H_
