#include "EaselComm2Test.h"

#include <utils/Errors.h>
#include <sstream>

#include "EaselComm2.h"
#include "EaselComm2Message.h"
#include "android-base/logging.h"
#include "easelcomm.h"
#include "third_party/halide/paintbox/src/runtime/imx.h"
#include "vendor/google_paintbox/test_infra/tests/libeaselcomm/test.pb.h"

// Test server for EaselComm2ImplTest.
// Must be copied to Easel and run before running EaselComm2ImplTest on AP.
// Server could host multiple test runs on AP and will not end when test ends.

using namespace android;

namespace {

ImxMemoryAllocatorHandle allocator;
std::unique_ptr<EaselComm2::Comm> server;

// Handles protobuffer calculation requests.
void handleProtoMessage(const EaselComm2::Message& message2) {
  test::Request request;
  CHECK(message2.toProto(&request));

  test::Response response;
  for (auto& mathOp : request.operations()) {
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

  CHECK_EQ(server->send(kProtoChannel, response), NO_ERROR);
}

// Handles string handshaking.
void handleStringMessage(const EaselComm2::Message& message2) {
  if (message2.toString() == "SYNC") {
    CHECK_EQ(server->send(kStringChannel, "ACK"), NO_ERROR);
  }
}

// Handles TestStruct reversing.
void handleStructMessage(const EaselComm2::Message& message2) {
  auto t = message2.toStruct<TestStruct>();
  CHECK(t != nullptr);

  TestStruct reverse = *t;
  reverse.number *= -1;
  reverse.flag = !reverse.flag;

  CHECK_EQ(server->send(kStructChannel, &reverse, sizeof(reverse)), NO_ERROR);
}

// Handles DMA ion buffer and echo same buffer back.
void handleIonBufferMessage(const EaselComm2::Message& message2) {
  ImxDeviceBufferHandle buffer;
  size_t size = message2.getPayload().size;
  CHECK_EQ(
      ImxCreateDeviceBufferManaged(allocator, size,
                                   kImxDefaultDeviceBufferAlignment,
                                   kImxDefaultDeviceBufferHeap, 0, &buffer),
      IMX_SUCCESS);

  int fd;
  CHECK_EQ(ImxShareDeviceBuffer(buffer, &fd), IMX_SUCCESS);

  // Receives the DMA to ImxDeviceBuffer.
  EaselComm2::HardwareBuffer hardwareBuffer(fd, size, 0);
  CHECK_EQ(server->receivePayload(message2, &hardwareBuffer), NO_ERROR);
  // Replies the same message.
  CHECK_EQ(server->send(kIonBufferChannel, {hardwareBuffer}), NO_ERROR);

  CHECK_EQ(ImxDeleteDeviceBuffer(buffer), IMX_SUCCESS);
}

// Handles DMA malloc buffer and echo same buffer back.
void handleMallocBufferMessage(const EaselComm2::Message& message2) {
  ImxDeviceBufferHandle buffer;
  size_t size = message2.getPayload().size;
  void* vaddr = malloc(size);
  CHECK(vaddr != nullptr);

  EaselComm2::HardwareBuffer hardwareBuffer(vaddr, size, 0);
  CHECK_EQ(server->receivePayload(message2, &hardwareBuffer), NO_ERROR);
  // Replies the same message.
  CHECK_EQ(server->send(kMallocBufferChannel, {hardwareBuffer}), NO_ERROR);
  free(vaddr);
}

}  // namespace

int main() {
  CHECK_EQ(ImxGetMemoryAllocator(IMX_MEMORY_ALLOCATOR_DEFAULT, &allocator),
           IMX_SUCCESS);

  server = EaselComm2::Comm::create(EaselComm2::Comm::Mode::SERVER);

  // Repeatedly receives test requests.
  while (true) {
    CHECK_EQ(server->open(EASEL_SERVICE_TEST), android::NO_ERROR);

    server->registerHandler(kIonBufferChannel, handleIonBufferMessage);
    server->registerHandler(kMallocBufferChannel, handleMallocBufferMessage);
    server->registerHandler(kProtoChannel, handleProtoMessage);
    server->registerHandler(kStructChannel, handleStructMessage);
    server->registerHandler(kStringChannel, handleStringMessage);

    CHECK_EQ(server->startReceiving(), android::NO_ERROR);
    server->joinReceiving();
    server->close();
  }
}
