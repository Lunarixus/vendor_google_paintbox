#ifndef HARDWARE_GCHIPS_PAINTBOX_SYSTEM_INCLUDE_CAPTURE_H_
#define HARDWARE_GCHIPS_PAINTBOX_SYSTEM_INCLUDE_CAPTURE_H_

#include <memory>
#include <vector>

#include "third_party/halide/paintbox/src/runtime/imx.h"

// Header file for Easel Mipi Capture API (go/easel-capture-api).
// This header is exposed to Android.

namespace paintbox {

// Definition of MIPI CSI-2 Standard Data-Types specification.
enum MipiDataTypeCsi2 {
  RAW8 = 0x2A,
  RAW10 = 0x2B,
  RAW16 = 0x2E,
};

// Definition of ports of Easel MIPI RX.
enum MipiRxPort {
  RX0 = 0,
  RX1 = 1,
  RX2 = 2,
};

// Capture configuration for one IPU MIPI stream.
struct CaptureStreamConfig {
  // MIPI CSI data type codes. e.g. 0x2B for Raw 10.
  MipiDataTypeCsi2 data_type;
  // Stream width in pixel, e.g. 4032 for 12MP stream.
  uint32_t width;
  // Stream height in pixel, e.g. 3032 for 12MP stream.
  uint32_t height;
  // Bits used for each pixel, e.g. 10 for Raw 10.
  uint32_t bits_per_pixel;
  // Whether buffer allocation is aligned to 64bits bus width.
  bool bus_aligned;
};

// Capture configuration for one frame.
struct CaptureConfig {
  // The mipi rx port of the capture, e.g. MipiRxPort::RX0.
  MipiRxPort rx_port;
  // Virtual channel id of the capture, e.g. 0.
  uint32_t virtual_channel_id;
  // Timeout value specified for the capture in ms, e.g. 50.
  uint32_t timeout_ms;
  // A list of stream configs specifying data types in a frame.
  std::vector<CaptureStreamConfig> stream_config_list;
};

// Error code for Capture API.
enum class CaptureError {
  SUCCESS,
  GENERIC_FAILURE,     // Generic failure.
  INVALID_REQUEST,     // Invalid request (e.g. internally inconsistent).
  NO_DEV,              // Device allocation failed.
  NO_MEM,              // Memory allocation failed.
  TIMEOUT,             // Request timed out (e.g. while waiting for interrupt).
  RESOURCE_NOT_FOUND,  // Resource not found.
  TYPE_MISMATCH,       // Type doesn't match.
  DATA_OVERFLOW,   // Data transfer/stream overflow; typically with MIPI Input.
  MUX_ERROR,       // MIPI mux error.
  THREAD_ERROR,    // Capture background thread error.
  ALREADY_INITED,  // Service already initialized.
  INVALID_CONFIG,  // Capture configuration is invalid.
  FS_ERROR,        // File system error.
  DATA_TYPE_NOT_FOUND,  // Data type not found.
  INVALID_BUFFER,       // Buffer is invalid.
  CANCEL,               // Capture is cancelled by user.
  UNKNOWN,              // Unknown error.
};

// Returns the error description of the error.
const char *GetCaptureErrorDesc(CaptureError error);

// Status of a frame buffer.
enum class CaptureStatus {
  // Frame buffer created, physical buffers allocated.
  CREATED,
  // Frame buffer enqueued for capturing, not available to user.
  ENQUEUED,
  // Frame buffer under capturing, not available to user.
  RUNNING,
  // Frame buffer capturing completed, available to user.
  COMPLETED,
};

// A CaptureFrameBuffer represents the data structure of a frame.
// CaptureFrameBuffer contains informaiton about frame status, timestamp and
// buffer handles tor retrieve captured data.
// CaptureFrameBuffer may contain multiple physical buffers for different data
// types.
// TODO(cjluo): Adds API to get ion handle.
class CaptureFrameBuffer {
 public:
  CaptureFrameBuffer(const CaptureFrameBuffer&) = delete;
  CaptureFrameBuffer& operator=(const CaptureFrameBuffer&) = delete;

  virtual ~CaptureFrameBuffer();

  // Returns the raw ImxDeviceBufferHandle of a data type.
  virtual ImxDeviceBufferHandle GetBufferHandle(int data_type) const = 0;

  // Returns the current status of the frame buffer.
  virtual CaptureStatus GetStatus() const = 0;

  // Returns the start timestamp as easel boot time in ns, 0 if frame is not
  // valid.
  virtual int64_t GetTimestampStartNs() const = 0;

  // Returns the end timestamp as easel boot time in ns, 0 if frame is not
  // valid.
  virtual int64_t GetTimestampEndNs() const = 0;

  // Returns the error code of capture.
  virtual CaptureError GetError() const = 0;

  // Returns all the data types registered in this frame buffer.
  virtual const std::vector<int> GetDataTypeList() const = 0;

  // Locks the buffer and gets the mapped void * of the data.
  // data_type the data_type of the specified buffer.
  // data data is the return value as the pointer towards the virtual address.
  virtual CaptureError LockFrameData(int data_type, void **data) const = 0;

  // Unlocks the buffer.
  // data_type the data_type of the specified buffer.
  virtual CaptureError UnlockFrameData(int data_type) const = 0;

  // Returns the row stride in bytes.
  virtual uint64_t GetRowStrideBytes(int data_type) const = 0;

 protected:
  CaptureFrameBuffer();
};

class CaptureFrameBufferFactory {
 public:
  CaptureFrameBufferFactory(const CaptureFrameBufferFactory&) = delete;
  CaptureFrameBufferFactory& operator=(
      const CaptureFrameBufferFactory&) = delete;

  virtual ~CaptureFrameBufferFactory();

  // Creates a default implementation of CaptureFrameBufferFactory.
  static std::unique_ptr<CaptureFrameBufferFactory> CreateInstance(
      const CaptureConfig &config);

  // Creates a new CaptureFrameBuffer.
  // Returns the created CaptureFrameBuffer unique ptr or nullptr if error
  // occurred.
  virtual std::unique_ptr<CaptureFrameBuffer> Create() = 0;

 protected:
  CaptureFrameBufferFactory();
};

// CaptureService is the key logic that calls IMX API to configure the IPU
// hardware to save frame from MIPI, process the frame and save to DRAM.
// This could further be extended to HalideCaptureService, VisaCaptureService,
// etc.
// A capture thread is created when this class is instantiated to handle the
// capture requests.
class CaptureService {
 public:
  CaptureService(const CaptureService&) = delete;
  CaptureService& operator=(const CaptureService&) = delete;

  virtual ~CaptureService();

  // Creates a default implementation of CaptureService.
  static std::unique_ptr<CaptureService> CreateInstance(
      const CaptureConfig &config);

  // Enqueues a frame buffer to the pending queue for capture.
  // This call could be called multiple times on the same frame_buffer.
  // frame_buffer frame buffer to store the to-be-captured frame.
  // Once frame_buffer is enqueued, the buffer is automatically unlocked.
  // To start capture, at least 2 frames needs to be enqueued.
  // Returns CaptureError::INVALID_BUFFER is buffer is invalid.
  virtual CaptureError EnqueueRequest(CaptureFrameBuffer* frame_buffer) = 0;

  // Dequeues a completed capture frame.
  // This call will block if currently there is no completed capture available.
  virtual CaptureFrameBuffer* DequeueCompletedRequest() = 0;

  // Clears the pending capture requests.
  virtual void ClearPendingRequests() = 0;

  // Pauses the current capturing.
  // Waits until the outstanding capture is finished.
  // Enqueued request will not be cleared.
  virtual void Pause() = 0;

  // Resumes the capturing.
  virtual void Resume() = 0;

 protected:
  CaptureService();
};
}  // namespace paintbox
#endif  // HARDWARE_GCHIPS_PAINTBOX_SYSTEM_INCLUDE_CAPTURE_H_
