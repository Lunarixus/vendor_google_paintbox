#define LOG_TAG "EaselExecutor"

#include "CpuExecutor.h"
#include "EaselExecutor.h"
#include "Rpc.h"

namespace paintbox_nn {

EaselExecutor::EaselExecutor() {
  mComm = EaselComm2::Comm::create(EaselComm2::Comm::Mode::SERVER);
  mPrepared = false;
}

void EaselExecutor::start() {
  mComm->registerHandler(
      PREPARE_MODEL, [&](const EaselComm2::Message& message) {
        handlePrepareModel(message);
      });

  mComm->registerHandler(
      EXECUTE, [&](const EaselComm2::Message& message) {
        handleExecute(message);
      });

  while (true) {
    mComm->open(EASEL_SERVICE_NN);
    mComm->startReceiving();
    mComm->joinReceiving();
    LOG(INFO) << "Client closes.";
    mComm->close();
  }
}

void EaselExecutor::handlePrepareModel(const EaselComm2::Message& message) {
  LOG(INFO) << "received PrepareModel";
  CHECK(message.toProto(&mModel));
  LOG(INFO) << "PrepareModel done. model size " << mModel.ByteSize();
  mPrepared = true;
}

void EaselExecutor::handleExecute(const EaselComm2::Message& message) {
  LOG(INFO) << "received Execute";
  CHECK(mPrepared);

  Request request;
  CHECK(message.toProto(&request));
  LOG(INFO) << "request size " << request.ByteSize();

  CHECK(message.hasPayload());
  size_t inputSize = message.getPayload().size;
  CHECK_EQ(inputSize, request.poolsizes(INPUT_POOL));

  // Receive input data.
  void* inputBuffer = malloc(inputSize);
  CHECK(inputBuffer != nullptr);
  EaselComm2::HardwareBuffer hardwareBuffer(inputBuffer, inputSize);
  CHECK_EQ(mComm->receivePayload(message, &hardwareBuffer), 0);
  LOG(INFO) << "request input buffer size " << inputSize;
  RunTimePoolInfo inputPoolInfo = {static_cast<uint8_t*>(inputBuffer)};

  // Allocate output data pool.
  size_t outputSize = request.poolsizes(OUTPUT_POOL);
  void* outputBuffer = malloc(outputSize);
  CHECK(inputBuffer != nullptr);
  RunTimePoolInfo outputPoolInfo = {static_cast<uint8_t*>(outputBuffer)};

  // Execute request.
  std::vector<RunTimePoolInfo> poolInfos;
  poolInfos.push_back(inputPoolInfo);
  poolInfos.push_back(outputPoolInfo);
  CpuExecutor executor;

  int res = executor.run(mModel, request, poolInfos);

  // Return result and output data.
  Response response;
  response.set_result(res);
  EaselComm2::HardwareBuffer resultBuffer(outputBuffer, outputSize);
  CHECK_EQ(mComm->send(EXECUTE, response, &resultBuffer), 0);

  free(inputBuffer);
  free(outputBuffer);
}

}  // namespace paintbox_nn
