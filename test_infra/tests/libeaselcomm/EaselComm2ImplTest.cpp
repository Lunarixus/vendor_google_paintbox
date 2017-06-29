#include <gtest/gtest.h>
#include <utils/Errors.h>
#include <vndk/hardware_buffer.h>

#include "easelcomm.h"
#include "vendor/google_paintbox/test_infra/tests/libeaselcomm/test.pb.h"

namespace android {
namespace {
using AHardwareBufferHandle = AHardwareBuffer*;

uint32_t getChannelSize(const AHardwareBuffer_Desc& desc) {
  if (desc.format == AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM) return 3;
  return 0;
}

size_t getBufferSize(const AHardwareBuffer_Desc& desc) {
  return desc.stride * desc.height * desc.layers * getChannelSize(desc);
}

uint8_t patternSimple(uint32_t x, uint32_t y, uint32_t c) {
  return (x + y * 2 + c * 3) % 256;
}

}  // namespace

// Test class for easelcomm2 usecases
class EaselComm2ImplTest : public ::testing::Test {
 public:
  // Allocates an AHardwareBuffer with format of RGB. This buffer is accessible from CPU rarely.
  // Returns NO_ERROR if successful, otherwise error code.
  int allocBuffer(uint32_t width, uint32_t height, AHardwareBufferHandle* buffer) {
    AHardwareBuffer_Desc desc = {
      width,
      height,
      /*layers=*/1,
      AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM,
      AHARDWAREBUFFER_USAGE_CPU_READ_RARELY | AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY,
      0,
      0,
      0,
    };
    return AHardwareBuffer_allocate(&desc, buffer);
  }

  // Releases the AHardwareBuffer.
  void releaseBuffer(AHardwareBufferHandle buffer) {
    if (buffer != nullptr) {
      AHardwareBuffer_release(buffer);
    }
  }

  // Repeatedly writes pattern to the buffer.
  // Returns NO_ERROR if successful, otherwise error code.
  int writePattern(
        std::function<uint8_t(uint32_t, uint32_t, uint32_t)> pattern,
        AHardwareBufferHandle buffer) {
    AHardwareBuffer_Desc desc;
    AHardwareBuffer_describe(buffer, &desc);

    void* vaddr;
    int ret = AHardwareBuffer_lock(
        buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY, -1, nullptr, &vaddr);
    if (ret != NO_ERROR) return ret;

    uint8_t* data = static_cast<uint8_t*>(vaddr);
    for (uint32_t y = 0; y < desc.height; y++) {
      uint32_t row_index = y * desc.stride;
      for (uint32_t x = 0; x < desc.width; x++) {
        uint32_t pixel_index = row_index + x;
        for (uint32_t c = 0; c < getChannelSize(desc); c++) {
          uint32_t byte_index = pixel_index * getChannelSize(desc) + c;
          data[byte_index] = pattern(x, y, c);
        }
      }
    }

    return AHardwareBuffer_unlock(buffer, nullptr);
  }

  // Checks if the buffer is filled with the pattern.
  // Returns NO_ERROR if checking passed otherwise BAD_VALUE.
  int checkPattern(
        std::function<uint8_t(uint32_t, uint32_t, uint32_t)> pattern,
        const AHardwareBufferHandle buffer) {
    AHardwareBuffer_Desc desc;
    AHardwareBuffer_describe(buffer, &desc);

    void* vaddr;
    int ret = AHardwareBuffer_lock(
        buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY, -1, nullptr, &vaddr);
    if (ret != NO_ERROR) return ret;

    bool match = true;
    uint8_t* data = static_cast<uint8_t*>(vaddr);
    for (uint32_t y = 0; y < desc.height; y++) {
      uint32_t row_index = y * desc.stride;
      for (uint32_t x = 0; x < desc.width; x++) {
        uint32_t pixel_index = row_index + x;
        for (uint32_t c = 0; c < getChannelSize(desc); c++) {
          uint32_t byte_index = pixel_index * getChannelSize(desc) + c;
          match &= (data[byte_index] == pattern(x, y, c));
          if (!match) break;
        }
        if (!match) break;
      }
      if (!match) break;
    }

    ret = AHardwareBuffer_unlock(buffer, nullptr);

    return match ? ret : BAD_VALUE;
  }

  // Sends the buffer to server.
  // Returns NO_ERROR if successful, otherwise error code.
  int sendBuffer(AHardwareBufferHandle buffer) {
    AHardwareBuffer_Desc desc;
    AHardwareBuffer_describe(buffer, &desc);

    EaselComm::EaselMessage message;
    message.message_buf = &desc;
    message.message_buf_size = sizeof(AHardwareBuffer_Desc);
    message.dma_buf = nullptr;
    message.dma_buf_fd = AHardwareBuffer_getNativeHandle(buffer)->data[0];
    message.dma_buf_type = EASELCOMM_DMA_BUFFER_DMA_BUF;
    message.dma_buf_size = getBufferSize(desc);

    return mClient.sendMessage(&message);
  }

  // Receives the buffer from server.
  // Returns NO_ERROR if successful, otherwise error code.
  int receiveBuffer(AHardwareBufferHandle* buffer) {
    EaselComm::EaselMessage message;
    int ret = mClient.receiveMessage(&message);
    if (ret != NO_ERROR) return ret;

    if (message.message_buf_size != sizeof(AHardwareBuffer_Desc)) return BAD_VALUE;
    AHardwareBuffer_Desc* desc = reinterpret_cast<AHardwareBuffer_Desc*>(message.message_buf);
    if (message.dma_buf_size != getBufferSize(*desc)) return BAD_VALUE;

    allocBuffer(desc->width, desc->height, buffer);
    message.dma_buf = nullptr;
    message.dma_buf_fd = AHardwareBuffer_getNativeHandle(*buffer)->data[0];
    message.dma_buf_type = EASELCOMM_DMA_BUFFER_DMA_BUF;

    ret = mClient.receiveDMA(&message);
    free(message.message_buf);
    return ret;
  }

  // Sends the proto buffer to server.
  // Returns NO_ERROR if successful, otherwise error code.
  int sendProtoBuffer(const test::Request& request) {
    size_t size = request.ByteSize();
    void* buffer = malloc(size);
    bool success = request.SerializeToArray(buffer, size);
    if (!success) return BAD_VALUE;

    EaselComm::EaselMessage message;
    message.message_buf = buffer;
    message.message_buf_size = size;
    message.dma_buf = nullptr;
    message.dma_buf_size = 0;

    int ret = mClient.sendMessage(&message);
    free(buffer);
    return ret;
  }

  // Receives the proto buffer from server.
  // Returns NO_ERROR if successful, otherwise error code.
  int receiveProtoBuffer(test::Response *response) {
    EaselComm::EaselMessage message;
    int ret = mClient.receiveMessage(&message);
    if (ret != NO_ERROR) return ret;
    bool success = response->ParseFromArray(message.message_buf, message.message_buf_size);
    free(message.message_buf);
    return success ? NO_ERROR : BAD_VALUE;
  }

 protected:
  void SetUp() override {
    ASSERT_EQ(mClient.open(EaselComm::EASEL_SERVICE_TEST), OK);
  }

  void TearDown() override {
    mClient.close();
  }

 private:
  EaselCommClient mClient;
};

TEST_F(EaselComm2ImplTest, AHardwareBufferLocalLoopback) {
  AHardwareBufferHandle buffer;
  ASSERT_EQ(allocBuffer(32, 24, &buffer), NO_ERROR);
  ASSERT_EQ(writePattern(patternSimple, buffer), NO_ERROR);
  EXPECT_EQ(checkPattern(patternSimple, buffer), NO_ERROR);
  releaseBuffer(buffer);
}

TEST_F(EaselComm2ImplTest, AHardareBufferEaselLoopback) {
  AHardwareBufferHandle txBuffer;
  ASSERT_EQ(allocBuffer(32, 24, &txBuffer), NO_ERROR);
  ASSERT_EQ(writePattern(patternSimple, txBuffer), NO_ERROR);
  ASSERT_EQ(sendBuffer(txBuffer), NO_ERROR);

  AHardwareBufferHandle rxBuffer;
  ASSERT_EQ(receiveBuffer(&rxBuffer), NO_ERROR);
  EXPECT_EQ(checkPattern(patternSimple, rxBuffer), NO_ERROR);

  releaseBuffer(txBuffer);
  releaseBuffer(rxBuffer);
}

TEST_F(EaselComm2ImplTest, MathRpc) {
  test::Request request;
  auto mathOp = request.add_operations();
  mathOp->set_op(test::MathOperation::ADD);
  mathOp->set_operand1(1);
  mathOp->set_operand2(2);

  mathOp = request.add_operations();
  mathOp->set_op(test::MathOperation::MINUS);
  mathOp->set_operand1(3);
  mathOp->set_operand2(4);

  mathOp = request.add_operations();
  mathOp->set_op(test::MathOperation::MULTIPLY);
  mathOp->set_operand1(5);
  mathOp->set_operand2(6);

  mathOp = request.add_operations();
  mathOp->set_op(test::MathOperation::DIVIDE);
  mathOp->set_operand1(7);
  mathOp->set_operand2(8);

  ASSERT_EQ(sendProtoBuffer(request), NO_ERROR);

  test::Response response;
  ASSERT_EQ(receiveProtoBuffer(&response), NO_ERROR);

  EXPECT_EQ(response.results_size(), 4);
  auto mathResult = response.results(0);
  EXPECT_EQ(mathResult.result(), 3);
  EXPECT_EQ(mathResult.expression(), "1 + 2 = 3");

  mathResult = response.results(1);
  EXPECT_EQ(mathResult.result(), -1);
  EXPECT_EQ(mathResult.expression(), "3 - 4 = -1");

  mathResult = response.results(2);
  EXPECT_EQ(mathResult.result(), 30);
  EXPECT_EQ(mathResult.expression(), "5 * 6 = 30");

  mathResult = response.results(3);
  EXPECT_EQ(mathResult.result(), 0);
  EXPECT_EQ(mathResult.expression(), "7 / 8 = 0");
}

}  // namespace androids