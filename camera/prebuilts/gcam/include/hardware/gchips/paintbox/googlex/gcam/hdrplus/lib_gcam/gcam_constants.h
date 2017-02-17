#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_GCAM_CONSTANTS_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_GCAM_CONSTANTS_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <cstdint>

namespace gcam {

// Absolute limits on the number of metering frames if we capture an explicit
// metering burst. Note that InitParams also puts a (potentially stricter)
// min/max bounds on the number of metering frames; the two values here
// are the absolute limits for the min/max in InitParams, which are what is
// actually enforced.
const int kMinFullMeteringSweepFrames = 1;
const int kMaxFullMeteringSweepFrames = 9;

// Absolute minimum legal number of images in the payload burst.
const int kMinPayloadFrames = 1;

// The default minimum number of payload frames for all devices.
// It is recommended to leave this at kMinPayloadFrames so that the tools
//   (burst_process and ae_test) have maximal flexibility when reprocessing
//   bursts; the Google camera app can set whatever value it likes.
// To set something other than the default, the client must modify
//   InitParams::min_payload_frames.
const int kDefaultMinPayloadFrames = kMinPayloadFrames;

// The default maximum number of payload frames for all devices. The raw
//   pipeline has no inherent limit on burst size.
// To set something other than the default, the client must modify
//   InitParams::max_payload_frames.
const int kDefaultMaxPayloadFrames = 16;

// The Gcam raw pipeline can only be used safely with input images that
// contain pixels in the range [0, kRawPixelMaxValue]. The pipeline does not
// explicitly assert this is true, but the pipeline is only tested with input
// data up to this value. Input pixels larger than this value may result in
// overflows of intermediate computations.
const int kRawPixelMaxValue = 1023;  // 10 bit data.

// Constants for special values of the {Static,Frame}Metadata::sensor_id flag.
//
// By convention, for a typical mobile device with two cameras, the rear-facing
// camera (which is often higher resolution and better optical quality) is
// designated the "primary" sensor, while the front-facing camera is designated
// the "secondary" sensor.
//
// For an array camera system, the camera designated the "primary" sensor is
// the one to which the images from other cameras in the array are aligned.
//
const int kSensorIdPrimary   = 0;
const int kSensorIdSecondary = 1;

// This value must never be provided by the caller as a burst_id.
const int kInvalidBurstId = -1;

// Constants to represent an image id that is ignored (e.g., When an input
// image can be null.).
const int64_t kInvalidImageId = -1;

// For every N images we get, throw out a blurry image (the next-blurriest in
// the whole burst).
// TODO(geiss): Study what % of shots are blurry from hand shake,
//       and then set this to a more-suitable 'N'.
// To disable, set to 999.
static const int kThrowOutOneInNBlurryImages = 6;

// Constant for the longest edge of postview output. This value is used if the
// client doesn't specify the desired postview output size.
const int kDefaultPostviewLongestEdge = 640;

// Constant slop factor that's used to handle rounding error while verifying
// the overall gain is in range.
const float kMaxOverallGainSlopFactor = 1.001f;

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_HDRPLUS_LIB_GCAM_GCAM_CONSTANTS_H_
