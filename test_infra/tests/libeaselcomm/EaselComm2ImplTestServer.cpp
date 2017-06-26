#include <utils/Errors.h>

#include "android-base/logging.h"
#include "easelcomm.h"
#include "imx.h"

// Test server for EaselComm2ImplTest.
// Must be copied to Easel and run before running EaselComm2ImplTest on AP.
// Server could host multiple test runs on AP and will not end when test ends.

// AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM has to match
// frameworks/native/libs/nativewindow/include/android/hardware_buffer.h
#define AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM 3

using namespace android;

namespace {
// AHardwareBuffer_Desc has to match
// frameworks/native/libs/nativewindow/include/android/hardware_buffer.h
typedef struct AHardwareBuffer_Desc {
    uint32_t    width;      // width in pixels
    uint32_t    height;     // height in pixels
    uint32_t    layers;     // number of images
    uint32_t    format;     // One of AHARDWAREBUFFER_FORMAT_*
    uint64_t    usage;      // Combination of AHARDWAREBUFFER_USAGE_*
    uint32_t    stride;     // Stride in pixels, ignored for AHardwareBuffer_allocate()
    uint32_t    rfu0;       // Initialize to zero, reserved for future use
    uint64_t    rfu1;       // Initialize to zero, reserved for future use
} AHardwareBuffer_Desc;

uint32_t getChannelSize(const AHardwareBuffer_Desc& desc) {
  if (desc.format == AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM) return 3;
  return 0;
}

size_t getBufferSize(const AHardwareBuffer_Desc& desc) {
  return desc.stride * desc.height * desc.layers * getChannelSize(desc);
}

ImxMemoryAllocatorHandle allocator;
EaselCommServer server;
}  // namespace

void messageHandlerThreadFunc(EaselComm::EaselMessage *msg) {
  CHECK_EQ(msg->message_buf_size, sizeof(AHardwareBuffer_Desc));

  AHardwareBuffer_Desc* desc = reinterpret_cast<AHardwareBuffer_Desc*>(msg->message_buf);
  CHECK_EQ(msg->dma_buf_size, getBufferSize(*desc));

  ImxDeviceBufferHandle buffer;
  CHECK_EQ(ImxCreateDeviceBufferManaged(
    allocator,
    getBufferSize(*desc),
    kImxDefaultDeviceBufferAlignment,
    kImxDefaultDeviceBufferHeap,
    0,
    &buffer), IMX_SUCCESS);

  int fd;
  CHECK_EQ(ImxShareDeviceBuffer(buffer, &fd), IMX_SUCCESS);
  msg->dma_buf = nullptr;
  msg->dma_buf_fd = fd;
  msg->dma_buf_type = EASELCOMM_DMA_BUFFER_DMA_BUF;

  // Receives the DMA to ImxDeviceBuffer.
  CHECK_EQ(server.receiveDMA(msg), NO_ERROR);
  // Replies the same message.
  CHECK_EQ(server.sendMessage(msg), NO_ERROR);

  CHECK_EQ(ImxDeleteDeviceBuffer(buffer), IMX_SUCCESS);
}

int main() {
  CHECK_EQ(ImxGetMemoryAllocator(IMX_MEMORY_ALLOCATOR_DEFAULT, &allocator), IMX_SUCCESS);

  // Repeatedly receives test requests.
  while (true) {
    CHECK_EQ(server.open(EaselComm::EASEL_SERVICE_TEST), android::NO_ERROR);
    CHECK_EQ(server.startMessageHandlerThread(messageHandlerThreadFunc), android::NO_ERROR);
    server.joinMessageHandlerThread();
    server.close();
  }
}
