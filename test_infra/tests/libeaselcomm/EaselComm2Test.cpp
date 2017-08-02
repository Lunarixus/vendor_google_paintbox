#include "EaselComm2Test.h"

#include <gtest/gtest.h>
#include <utils/Errors.h>
#include <vndk/hardware_buffer.h>
#include <condition_variable>
#include <mutex>

#include "EaselComm2.h"
#include "EaselComm2Message.h"
#include "easelcomm.h"
#include "vendor/google_paintbox/test_infra/tests/libeaselcomm/test.pb.h"

namespace android {
namespace {
using AHardwareBufferHandle = AHardwareBuffer*;

// Returns image channel counts, 0 for unsupported formats.
uint32_t getChannelSize(const AHardwareBuffer_Desc& desc) {
  if (desc.format == AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM) return 3;
  return 0;
}

uint8_t patternSimple(uint32_t x, uint32_t y, uint32_t c, uint32_t seed) {
  uint32_t seed2 = seed * seed;
  uint32_t seed3 = seed2 * seed;
  return static_cast<uint8_t>(x * seed + y * seed2 + c * seed3);
}

// HardwareBuffer does not take ownership of the input buffer.
EaselComm2::HardwareBuffer convertToHardwareBuffer(AHardwareBufferHandle buffer,
                                                   int id = 0) {
  AHardwareBuffer_Desc aDesc;
  AHardwareBuffer_describe(buffer, &aDesc);
  int fd = AHardwareBuffer_getNativeHandle(buffer)->data[0];
  size_t size =
      aDesc.stride * aDesc.height * aDesc.layers * getChannelSize(aDesc);
  EaselComm2::HardwareBuffer hardwareBuffer = {fd, size, id};
  return hardwareBuffer;
}
}  // namespace

// Test class for easelcomm2 usecases
class EaselComm2Test : public ::testing::Test {
 public:
  EaselComm2Test() {
    mClient = EaselComm2::Comm::create(EaselComm2::Comm::Mode::CLIENT);
  }

  // Allocates an AHardwareBuffer with format
  // AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM. This buffer is with usage
  // CPU_READ_RARELY and CPU_WRITE_RARELY.
  // Returns NO_ERROR if successful, otherwise error code.
  int allocBuffer(uint32_t width, uint32_t height,
                  AHardwareBufferHandle* bufferOut) {
    AHardwareBuffer_Desc desc = {
        width,
        height,
        /*layers=*/1,
        AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM,
        AHARDWAREBUFFER_USAGE_CPU_READ_RARELY |
            AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY,
        0,
        0,
        0,
    };
    return AHardwareBuffer_allocate(&desc, bufferOut);
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
      uint32_t seed,
      std::function<uint8_t(uint32_t, uint32_t, uint32_t, uint32_t)> pattern,
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
          data[byte_index] = pattern(x, y, c, seed);
        }
      }
    }

    return AHardwareBuffer_unlock(buffer, nullptr);
  }

  // Checks if the buffer is filled with the pattern.
  // Returns NO_ERROR if checking passed otherwise BAD_VALUE.
  int checkPattern(
      uint32_t seed,
      std::function<uint8_t(uint32_t, uint32_t, uint32_t, uint32_t)> pattern,
      const AHardwareBufferHandle buffer) {
    AHardwareBuffer_Desc desc;
    AHardwareBuffer_describe(buffer, &desc);

    void* vaddr;
    int ret = AHardwareBuffer_lock(
        buffer, AHARDWAREBUFFER_USAGE_CPU_READ_RARELY, -1, nullptr, &vaddr);
    if (ret != NO_ERROR) return ret;

    bool match = true;
    uint8_t* data = static_cast<uint8_t*>(vaddr);
    for (uint32_t y = 0; y < desc.height; y++) {
      uint32_t row_index = y * desc.stride;
      for (uint32_t x = 0; x < desc.width; x++) {
        uint32_t pixel_index = row_index + x;
        for (uint32_t c = 0; c < getChannelSize(desc); c++) {
          uint32_t byte_index = pixel_index * getChannelSize(desc) + c;
          match &= (data[byte_index] == pattern(x, y, c, seed));
          if (!match) break;
        }
        if (!match) break;
      }
      if (!match) break;
    }

    ret = AHardwareBuffer_unlock(buffer, nullptr);

    return match ? ret : BAD_VALUE;
  }

  EaselComm2::Comm* comm() { return mClient.get(); }

  void wait() {
    std::unique_lock<std::mutex> lock(mReceiveLock);
    mReceiveCond.wait(lock, [&] { return mReceived; });
  }

  void signal() {
    {
      std::unique_lock<std::mutex> lock(mReceiveLock);
      mReceived = true;
    }
    mReceiveCond.notify_one();
  }

 protected:
  void SetUp() override {
    mReceived = false;
    ASSERT_EQ(mClient->open(EASEL_SERVICE_TEST), OK);
    ASSERT_EQ(mClient->startReceiving(), OK);
  }

  void TearDown() override { mClient->close(); }

 private:
  std::unique_ptr<EaselComm2::Comm> mClient;
  std::mutex mReceiveLock;
  std::condition_variable mReceiveCond;
  bool mReceived;
};

TEST_F(EaselComm2Test, AHardwareBufferLocalLoopback) {
  AHardwareBufferHandle buffer;
  ASSERT_EQ(allocBuffer(32, 24, &buffer), NO_ERROR);
  ASSERT_EQ(writePattern(11, patternSimple, buffer), NO_ERROR);
  EXPECT_EQ(checkPattern(11, patternSimple, buffer), NO_ERROR);
  releaseBuffer(buffer);
}

TEST_F(EaselComm2Test, AHardareBufferEaselLoopback) {
  AHardwareBufferHandle txBuffer;
  ASSERT_EQ(allocBuffer(32, 24, &txBuffer), NO_ERROR);
  AHardwareBufferHandle rxBuffer;
  ASSERT_EQ(allocBuffer(32, 24, &rxBuffer), NO_ERROR);

  comm()->registerHandler(
      kBufferChannel, [&](const EaselComm2::Message& message) {
        ASSERT_TRUE(message.getHeader()->hasPayload);
        auto rxHardwareBuffer = convertToHardwareBuffer(rxBuffer);
        ASSERT_EQ(comm()->receivePayload(message, &rxHardwareBuffer), NO_ERROR);
        EXPECT_EQ(checkPattern(13, patternSimple, rxBuffer), NO_ERROR);
        signal();
      });

  ASSERT_EQ(writePattern(13, patternSimple, txBuffer), NO_ERROR);
  auto txHardwareBuffer = convertToHardwareBuffer(txBuffer);
  ASSERT_EQ(comm()->send(kBufferChannel, {txHardwareBuffer}), NO_ERROR);

  wait();

  releaseBuffer(txBuffer);
  releaseBuffer(rxBuffer);
}

TEST_F(EaselComm2Test, MultipleAHardareBufferEaselLoopback) {
  const int kSize = 5;

  AHardwareBufferHandle rxBuffer;
  ASSERT_EQ(allocBuffer(32, 24, &rxBuffer), NO_ERROR);

  int count = 0;
  comm()->registerHandler(
      kBufferChannel, [&](const EaselComm2::Message& message) {
        ASSERT_TRUE(message.getHeader()->hasPayload);
        auto rxHardwareBuffer = convertToHardwareBuffer(rxBuffer);
        ASSERT_EQ(comm()->receivePayload(message, &rxHardwareBuffer), NO_ERROR);
        EXPECT_EQ(checkPattern(static_cast<uint32_t>(rxHardwareBuffer.id),
                               patternSimple, rxBuffer),
                  NO_ERROR);
        count++;
        if (count >= kSize) signal();
      });

  std::vector<AHardwareBufferHandle> txBufferHandles;
  std::vector<EaselComm2::HardwareBuffer> txBuffers;

  for (int i = 0; i < kSize; i++) {
    AHardwareBufferHandle buffer;
    ASSERT_EQ(allocBuffer(32, 24, &buffer), NO_ERROR);
    int id = i + 17;
    ASSERT_EQ(writePattern(static_cast<uint32_t>(id), patternSimple, buffer),
              NO_ERROR);
    txBufferHandles.push_back(buffer);
    txBuffers.push_back(convertToHardwareBuffer(buffer, id));
  }

  ASSERT_EQ(comm()->send(kBufferChannel, txBuffers), NO_ERROR);

  wait();

  for (auto& handle : txBufferHandles) {
    releaseBuffer(handle);
  }
  releaseBuffer(rxBuffer);
}

TEST_F(EaselComm2Test, MathRpc) {
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

  comm()->registerHandler(kProtoChannel,
                          [&](const EaselComm2::Message& message) {
                            test::Response response;
                            ASSERT_TRUE(message.toProto(&response));

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

                            signal();
                          });

  ASSERT_EQ(comm()->send(kProtoChannel, request), NO_ERROR);
  wait();
}

TEST_F(EaselComm2Test, SyncAck) {
  comm()->registerHandler(kStringChannel,
                          [&](const EaselComm2::Message& message) {
                            EXPECT_EQ(message.toString(), "ACK");
                            signal();
                          });

  ASSERT_EQ(comm()->send(kStringChannel, "SYNC"), NO_ERROR);
  wait();
}

TEST_F(EaselComm2Test, Reverse) {
  comm()->registerHandler(kStructChannel,
                          [&](const EaselComm2::Message& message) {
                            auto t = message.toStruct<TestStruct>();
                            EXPECT_EQ(t->number, -10);
                            EXPECT_FALSE(t->flag);
                            signal();
                          });

  TestStruct t{10, true};
  ASSERT_EQ(comm()->send(kStructChannel, &t, sizeof(t)), NO_ERROR);
  wait();
}

}  // namespace android
