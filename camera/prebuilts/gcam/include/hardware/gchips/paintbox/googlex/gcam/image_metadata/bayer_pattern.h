#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_IMAGE_METADATA_BAYER_PATTERN_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_IMAGE_METADATA_BAYER_PATTERN_H_

#include <algorithm>
#include <cassert>
#include <cstdint>

namespace gcam {

enum class BayerPattern {
  kInvalid = 0,
  // Each of the following enum states describe an arrangement of color
  // filters on the sensor, with four codes (R = red, G = green, B = blue).
  // The four codes, in order, represent the color filters in the top-left
  // 2x2 pixels of the sensor, in reading order: upper-left, upper-right,
  // lower-left, and lower-right.
  kRGGB,
  kBGGR,
  kGRBG,   // (Less common.)
  kGBRG    // (Less common.)
};

const char* ToText(BayerPattern pattern);
BayerPattern TextToBayerPattern(const char* text);

BayerPattern GetRandomBayerPattern();

// Write the color channels for a given BayerPattern enum into a 4-element
// array, where (R,G,B) are written as (0,1,2) respectively. For example,
// BayerPattern::kGRGB is written as [1,0,1,2].
// Returns false on an invalid Bayer pattern, leaving the output unset.
bool BayerPatternColors(const BayerPattern& bayer_pattern,
                        uint8_t bayer_colors[4]);

// Get the BayerPattern enum corresponding to a 4-element array of color
// indices, where (0,1,2) map to (R,G,B) respectively. For example, [2,1,1,0]
// gets decoded to BayerPattern::kBGGR.
BayerPattern BayerPatternFromColors(const uint8_t bayer_colors[4]);

// Rearrange a 4-element array from R,Gr,Gb,B order to Bayer order (row-major
// "reading" order of the top-left 2x2 pixel block), according to the given
// Bayer pattern.
template <typename T>
void RggbToBayer(BayerPattern pattern, T* channels) {
  switch (pattern) {
    case BayerPattern::kRGGB:
      // Nothing to do.
      break;
    case BayerPattern::kBGGR:
      std::swap(channels[0], channels[3]);
      std::swap(channels[1], channels[2]);
      break;
    case BayerPattern::kGRBG:
      std::swap(channels[0], channels[1]);
      std::swap(channels[2], channels[3]);
      break;
    case BayerPattern::kGBRG:
      std::swap(channels[0], channels[2]);
      std::swap(channels[1], channels[3]);
      break;
    default:
      assert(false);
      break;
  }
}

// Reverse of the above.
template <typename T>
void BayerToRggb(BayerPattern pattern, T* channels) {
  // The functions are actually the same, but we provide both for clarity in the
  // calling code.
  RggbToBayer(pattern, channels);
}

// Similar to the above two functions, but do it out-of-place.
template <typename T>
void RggbToBayer(BayerPattern pattern, const T* rggb, T* bayer) {
  for (int ch = 0; ch < 4; ch++) {
    bayer[ch] = rggb[ch];
  }
  RggbToBayer(pattern, bayer);
}

template <typename T>
void BayerToRggb(BayerPattern pattern, const T* bayer, T* rggb) {
  // The functions are actually the same, but we provide both for clarity in the
  // calling code.
  RggbToBayer(pattern, bayer, rggb);
}

// Returns the x and y positions of the red, green and blue channels within
// a 2x2 pixel Bayer pattern cell.  The x and y values returned are either
// 0 or 1.  Position (0, 0) refers to the upper left corner of the cell.
// The green channels are ordered such that yGR is always equal to yR and
// yGB is always equal to yB.
void RggbPositions(BayerPattern pattern, int* xR, int* yR, int* xGR, int* yGR,
                   int* xGB, int* yGB, int* xB, int* yB);

// TODO(geiss): There are many places in the code that do this, and could be
// made more readable by calling this function instead.
template <typename T>
void RggbToRgb(const T rggb[4], T rgb_out[3]) {
  rgb_out[0] = rggb[0];
  rgb_out[1] = (rggb[1] + rggb[2]) / 2;
  rgb_out[2] = rggb[3];
}

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_IMAGE_METADATA_BAYER_PATTERN_H_
