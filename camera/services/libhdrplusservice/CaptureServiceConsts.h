#ifndef PAINTBOX_HDR_PLUS_CAPTURE_SERVICE_CONSTS_H
#define PAINTBOX_HDR_PLUS_CAPTURE_SERVICE_CONSTS_H

namespace pbcamera {

namespace capture_service_consts {

// Timeout for capture frame buffer factory.
// IPU may require upto 2 frame time periods to complete the first capture.
// Assuming max frame time period is 66ms, this number needs to be >= 132ms.
const uint32_t kCaptureFrameBufferFactoryTimeoutMs = 150;

// MIPI RAW10 data type.
const uint32_t kMipiRaw10DataType = 0x2b;

// Virtual channel ID for main image.
const uint32_t kMainImageVirtualChannelId = 0;

// If the stream should be bus aligned for capture service.
const bool kBusAlignedStreamConfig = true;

} // capture_service_consts

} // namespace pbcamera

#endif // PAINTBOX_HDR_PLUS_CAPTURE_SERVICE_CONSTS_H