#include <utils/Errors.h>
#include <sstream>

#include "EaselHardwareBuffer.h"
#include "android-base/logging.h"
#include "easelcomm.h"
#include "third_party/halide/paintbox/src/runtime/imx.h"
#include "vendor/google_paintbox/test_infra/tests/libeaselcomm/test.pb.h"

// Test server for EaselComm2ImplTest.
// Must be copied to Easel and run before running EaselComm2ImplTest on AP.
// Server could host multiple test runs on AP and will not end when test ends.

// AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM has to match
// frameworks/native/libs/nativewindow/include/android/hardware_buffer.h
#define AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM 3

using namespace android;

namespace {

ImxMemoryAllocatorHandle allocator;
EaselCommServer server;

// Handles protobuffer calculation requests.
void handleProtoMessage(EaselComm::EaselMessage *msg) {
  CHECK(msg->message_buf_size > 0);
  CHECK(msg->message_buf);

  test::Request request;
  CHECK(request.ParseFromArray(msg->message_buf, msg->message_buf_size));

  test::Response response;
  for (int i = 0; i < request.operations_size(); i++) {
    auto mathOp = request.operations(i);
    char op;
    auto mathResult = response.add_results();
    switch (mathOp.op()) {
      case test::MathOperation::ADD:
        mathResult->set_result(mathOp.operand1() + mathOp.operand2());
        op = '+';
        break;
      case test::MathOperation::MINUS:
        mathResult->set_result(mathOp.operand1() - mathOp.operand2());
        op = '-';
        break;
      case test::MathOperation::MULTIPLY:
        mathResult->set_result(mathOp.operand1() * mathOp.operand2());
        op = '*';
        break;
      case test::MathOperation::DIVIDE:
        mathResult->set_result(mathOp.operand1() / mathOp.operand2());
        op = '/';
        break;
    }
    std::stringstream ss;
    ss << mathOp.operand1() << " " << op << " " << mathOp.operand2() << " = "
       << mathResult->result();
    mathResult->set_expression(ss.str());
  }

  size_t size = response.ByteSize();
  void *buffer = malloc(size);
  CHECK(response.SerializeToArray(buffer, size));

  EaselComm::EaselMessage reply;
  reply.message_buf = buffer;
  reply.message_buf_size = size;
  reply.dma_buf = nullptr;
  reply.dma_buf_size = 0;

  CHECK_EQ(server.sendMessage(&reply), NO_ERROR);
  free(buffer);
}

// Handles DMA ion buffer and echo same buffer back.
void handleBufferMessage(EaselComm::EaselMessage *msg) {
  CHECK_EQ(msg->message_buf_size, sizeof(EaselComm2::HardwareBuffer::Desc));

  auto desc =
      reinterpret_cast<EaselComm2::HardwareBuffer::Desc *>(msg->message_buf);
  CHECK_EQ(msg->dma_buf_size, EaselComm2::HardwareBuffer::size(*desc));

  ImxDeviceBufferHandle buffer;
  CHECK_EQ(ImxCreateDeviceBufferManaged(
               allocator, EaselComm2::HardwareBuffer::size(*desc),
               kImxDefaultDeviceBufferAlignment, kImxDefaultDeviceBufferHeap, 0,
               &buffer),
           IMX_SUCCESS);

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

void messageHandlerThreadFunc(EaselComm::EaselMessage *msg) {
  if (msg->dma_buf_size == 0) {
    handleProtoMessage(msg);
  } else {
    handleBufferMessage(msg);
  }
}
}  // namespace

int main() {
  CHECK_EQ(ImxGetMemoryAllocator(IMX_MEMORY_ALLOCATOR_DEFAULT, &allocator),
           IMX_SUCCESS);

  // Repeatedly receives test requests.
  while (true) {
    CHECK_EQ(server.open(EaselComm::EASEL_SERVICE_TEST), android::NO_ERROR);
    CHECK_EQ(server.startMessageHandlerThread(messageHandlerThreadFunc),
             android::NO_ERROR);
    server.joinMessageHandlerThread();
    server.close();
  }
}
