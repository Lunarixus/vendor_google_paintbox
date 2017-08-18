/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "PaintboxDriver"

#include "Conversion.h"
#include "CpuExecutor.h"
#include "HalInterfaces.h"
#include "PaintboxDriver.h"
#include "Rpc.h"
#include "vendor/google_paintbox/nn/shared/proto/types.pb.h"

namespace android {
namespace nn {
namespace paintbox_driver {

Return<void> PaintboxDriver::initialize(initialize_cb cb) {
  LOG(DEBUG) << "PaintboxDriver::initialize()";

  // Our driver supports every op.
  hidl_vec<OperationType> supportedOperationTypes{
      OperationType::AVERAGE_POOL_FLOAT32,
      OperationType::CONCATENATION_FLOAT32,
      OperationType::CONV_FLOAT32,
      OperationType::DEPTHWISE_CONV_FLOAT32,
      OperationType::MAX_POOL_FLOAT32,
      OperationType::L2_POOL_FLOAT32,
      OperationType::DEPTH_TO_SPACE_FLOAT32,
      OperationType::SPACE_TO_DEPTH_FLOAT32,
      OperationType::LOCAL_RESPONSE_NORMALIZATION_FLOAT32,
      OperationType::SOFTMAX_FLOAT32,
      OperationType::RESHAPE_FLOAT32,
      OperationType::SPLIT_FLOAT32,
      OperationType::FAKE_QUANT_FLOAT32,
      OperationType::ADD_FLOAT32,
      OperationType::FULLY_CONNECTED_FLOAT32,
      OperationType::CAST_FLOAT32,
      OperationType::MUL_FLOAT32,
      OperationType::L2_NORMALIZATION_FLOAT32,
      OperationType::LOGISTIC_FLOAT32,
      OperationType::RELU_FLOAT32,
      OperationType::RELU6_FLOAT32,
      OperationType::RELU1_FLOAT32,
      OperationType::TANH_FLOAT32,
      OperationType::DEQUANTIZE_FLOAT32,
      OperationType::FLOOR_FLOAT32,
      OperationType::GATHER_FLOAT32,
      OperationType::RESIZE_BILINEAR_FLOAT32,
      OperationType::LSH_PROJECTION_FLOAT32,
      OperationType::LSTM_FLOAT32,
      OperationType::SVDF_FLOAT32,
      OperationType::RNN_FLOAT32,
      OperationType::N_GRAM_FLOAT32,
      OperationType::LOOKUP_FLOAT32,
  };

  // TODO: These numbers are completely arbitrary.  To be revised.
  PerformanceInfo float16Performance = {
      .execTime = 116.0f,  // nanoseconds?
      .powerUsage = 1.0f,  // picoJoules
  };

  PerformanceInfo float32Performance = {
      .execTime = 132.0f,  // nanoseconds?
      .powerUsage = 1.0f,  // picoJoules
  };

  PerformanceInfo quantized8Performance = {
      .execTime = 100.0f,  // nanoseconds?
      .powerUsage = 1.0f,  // picoJoules
  };

  Capabilities capabilities = {
      .supportedOperationTypes = supportedOperationTypes,
      .cachesCompilation = false,
      .bootupTime = 1e-3f,
      .float16Performance = float16Performance,
      .float32Performance = float32Performance,
      .quantized8Performance = quantized8Performance,
  };

  mComm = EaselComm2::Comm::create(EaselComm2::Comm::Mode::CLIENT);

  // return
  cb(capabilities);
  return Void();
}

Return<void> PaintboxDriver::getSupportedSubgraph(
    [[maybe_unused]] const Model& model, getSupportedSubgraph_cb cb) {
  LOG(DEBUG) << "PaintboxDriver::getSupportedSubgraph()";
  std::vector<bool> canDo;  // TODO implement
  if (validateModel(model)) {
    // TODO
  }
  cb(canDo);
  return Void();
}

Return<sp<IPreparedModel>> PaintboxDriver::prepareModel(const Model& model) {
  if (!validateModel(model)) {
    return nullptr;
  }

  mComm->close();
  mComm->open(EASEL_SERVICE_NN);
  mComm->startReceiving();

  paintbox_nn::Model protoModel;
  paintbox_util::convertHidlModel(model, &protoModel);
  mComm->send(PREPARE_MODEL, protoModel);

  // TODO(cjluo): Add support for model pools.

  LOG(DEBUG) << "PaintboxDriver::prepareModel(" << toString(model) << ")";
  return new PaintboxPreparedModel(model, mComm.get());
}

Return<DeviceStatus> PaintboxDriver::getStatus() {
  LOG(DEBUG) << "PaintboxDriver::getStatus()";
  return DeviceStatus::AVAILABLE;
}

PaintboxPreparedModel::PaintboxPreparedModel(const Model& model,
                                             EaselComm2::Comm* comm) {
  // Make a copy of the model, as we need to preserve it.
  mModel = model;
  mComm = comm;

  mExecuteDone = false;
  mExecuteReturn = ANEURALNETWORKS_NO_ERROR;
  mOutput = nullptr;
}

static bool mapPools(std::vector<RunTimePoolInfo>* poolInfos,
                     const hidl_vec<hidl_memory>& pools) {
  poolInfos->resize(pools.size());
  for (size_t i = 0; i < pools.size(); i++) {
    auto& poolInfo = (*poolInfos)[i];
    poolInfo.memory = mapMemory(pools[i]);
    if (poolInfo.memory == nullptr) {
      LOG(ERROR) << "PaintboxDriver Can't create shared memory.";
      return false;
    }
    poolInfo.memory->update();
    poolInfo.buffer = reinterpret_cast<uint8_t*>(
        static_cast<void*>(poolInfo.memory->getPointer()));
    if (poolInfo.buffer == nullptr) {
      LOG(ERROR)
          << "PaintboxPreparedModel::execute Can't create shared memory.";
      return false;
    }
  }
  return true;
}

void PaintboxPreparedModel::executeCallback(
    const EaselComm2::Message& message) {
  paintbox_nn::Response response;
  CHECK(message.toProto(&response));
  LOG(INFO) << "PaintboxPreparedModel::executeCallback result "
            << response.result();

  CHECK(message.hasPayload());
  size_t outputSize = message.getPayload().size;

  std::unique_lock<std::mutex> executeLock(mExecuteMutex);
  CHECK_EQ(outputSize, static_cast<size_t>(mOutput->memory->getSize()));
  EaselComm2::HardwareBuffer hardwareBuffer(mOutput->buffer, outputSize);
  CHECK_EQ(mComm->receivePayload(message, &hardwareBuffer), 0);
  mExecuteReturn = response.result();
  mExecuteDone = true;
  mExecuteDoneCond.notify_one();
}

Return<bool> PaintboxPreparedModel::execute(const Request& request) {
  LOG(DEBUG) << "PaintboxDriver::prepareRequest(" << toString(request) << ")";

  // Convert hidl request to proto request.
  paintbox_nn::Request protoRequest;
  paintbox_util::convertHidlRequest(request, &protoRequest);

  // Convert input data buffer to EaselComm2::HardwareBuffer.
  std::vector<RunTimePoolInfo> poolInfo;
  if (!mapPools(&poolInfo, request.pools)) {
    return false;
  }
  auto& input = poolInfo[INPUT_POOL];
  EaselComm2::HardwareBuffer hardwareBuffer(
      input.buffer, static_cast<size_t>(input.memory->getSize()));

  // Register execute handler.
  mComm->registerHandler(
      EXECUTE, [&](const EaselComm2::Message& message) {
        executeCallback(message);
      });

  // Send request to Easel and wait for response.
  std::unique_lock<std::mutex> executeLock(mExecuteMutex);
  mExecuteDone = false;
  mExecuteReturn = ANEURALNETWORKS_NO_ERROR;
  mOutput = &poolInfo[OUTPUT_POOL];
  mComm->send(EXECUTE, protoRequest, &hardwareBuffer);
  mExecuteDoneCond.wait(executeLock, [&] { return mExecuteDone; });

  LOG(DEBUG) << "executor.run returned " << mExecuteReturn;
  return mExecuteReturn == ANEURALNETWORKS_NO_ERROR;
}

}  // namespace paintbox_driver
}  // namespace nn
}  // namespace android
