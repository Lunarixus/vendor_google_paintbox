#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_RAW_FINISH_FINISH_TUNING_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_RAW_FINISH_FINISH_TUNING_H_

namespace gcam {

// How many points define a curve used for sharpening.
static const int kRawSharpenCurveSize = 4;

// The size of the YUV -> UV LUT used to tune Chroma. This measures the number
// of bins in the LUT (the number of values/vertices in the LUT is one larger).
// This cannot be changed without changing the LUT application code in
// chroma_adjustment_halide.cc.
static const int kLog2ChromaLutSizeY = 2;
static const int kLog2ChromaLutSizeUV = 3;
static const int kChromaLutSizeY = 1 << kLog2ChromaLutSizeY;
static const int kChromaLutSizeUV = 1 << kLog2ChromaLutSizeUV;

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_RAW_FINISH_FINISH_TUNING_H_
