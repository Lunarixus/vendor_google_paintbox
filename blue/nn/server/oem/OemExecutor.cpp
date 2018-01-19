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

#define LOG_TAG "OemExecutor"

#include "OemExecutor.h"

#include "MatrixAddEngine.h"
#include "MobileNetBodyEngine.h"
#include "NeuralNetworks.h"
#include "OemModel.h"
#include "Operations.h"

#include <sys/mman.h>

namespace paintbox_nn {

// Updates the RunTimeOperandInfo with the newly calculated shape.
// Allocate the buffer if we need to.
bool RunTimeOperandInfo::setInfoAndAllocateIfNeeded(const Shape& shape) {
  // For user-provided model output operands, the parameters must match the
  // Shape calculated from the preparation step.
  if (lifetime == OperandLifeTime::MODEL_OUTPUT) {
    if (type != shape.type || dimensions != shape.dimensions) {
      LOG(ERROR) << "Invalid type or dimensions for model output";
      return false;
    }
    if (type == OperandType::TENSOR_QUANT8_ASYMM &&
        (scale != shape.scale || zeroPoint != shape.offset)) {
      LOG(ERROR) << "Invalid scale or zeroPoint for model output";
      return false;
    }
  }
  type = shape.type;
  dimensions = shape.dimensions;
  scale = shape.scale;
  zeroPoint = shape.offset;
  if (lifetime == OperandLifeTime::TEMPORARY_VARIABLE && buffer == nullptr) {
    uint32_t length = sizeOfData(type, dimensions);
    buffer = new uint8_t[length];
    if (buffer == nullptr) {
      return false;
    }
  }
  return true;
}

OemExecutor::OemExecutor(std::unique_ptr<Model> model) {
  CHECK(model != nullptr);
  mModel = std::move(model);
  mModelPoolInfos.resize(mModel->poolsizes().size(), {nullptr, 0});
}

OemExecutor::~OemExecutor() {
  for (RunTimePoolInfo& info : mModelPoolInfos) {
    if (info.buffer != nullptr) free(info.buffer);
  }
}

RunTimePoolInfo* OemExecutor::allocModelPoolInfo(size_t index) {
  if (index >= mModelPoolInfos.size()) return nullptr;

  auto poolSize = mModel->poolsizes(index);
  void* pool = malloc(poolSize);
  if (pool == nullptr) return nullptr;
  mModelPoolInfos[index] = {reinterpret_cast<uint8_t*>(pool),
                            static_cast<size_t>(poolSize)};

  return &(mModelPoolInfos[index]);
}

bool OemExecutor::ready() {
  for (RunTimePoolInfo& info : mModelPoolInfos) {
    if (info.size == 0) return false;
    if (info.buffer == nullptr) return false;
  }
  return true;
}

// Ignore the .pools entry in model and request.  This will have been taken care
// of by the caller.
int OemExecutor::run(const Request& request,
                     const std::vector<RunTimePoolInfo>& requestPoolInfos) {
  LOG(INFO) << "OemExecutor::run()";

  std::vector<RunTimeOperandInfo> operands;
  bool success = initializeRunTimeInfo(request, requestPoolInfos, &operands);
  if (!success) return ANEURALNETWORKS_BAD_DATA;
  // The model has serialized the operation in execution order.
  for (const auto& operation : mModel->operations()) {
    int n = executeOperation(operation, &operands);
    if (n != ANEURALNETWORKS_NO_ERROR) {
      return n;
    }
  }

  LOG(INFO) << "Completed run normally";
  return ANEURALNETWORKS_NO_ERROR;
}

bool OemExecutor::initializeRunTimeInfo(
    const Request& request,
    const std::vector<RunTimePoolInfo>& requestPoolInfos,
    std::vector<RunTimeOperandInfo>* operands) {
  LOG(INFO) << "OemExecutor::initializeRunTimeInfo";
  const size_t count = mModel->operands().size();
  operands->resize(count);

  // Start by setting the runtime info to what's in the model.
  for (size_t i = 0; i < count; i++) {
    const Operand& from = mModel->operands(i);
    RunTimeOperandInfo& to = operands->at(i);
    to.type = from.type();
    to.dimensions = {from.dimensions().begin(), from.dimensions().end()};
    to.scale = from.scale();
    to.zeroPoint = from.zeropoint();
    to.length = from.location().length();
    to.lifetime = from.lifetime();
    switch (from.lifetime()) {
      case OperandLifeTime::TEMPORARY_VARIABLE:
        to.buffer = nullptr;
        to.numberOfUsesLeft = from.numberofconsumers();
        break;
      case OperandLifeTime::CONSTANT_COPY:
        to.buffer = reinterpret_cast<uint8_t*>(
            const_cast<char*>(mModel->operandvalues().c_str()) +
            from.location().offset());
        to.numberOfUsesLeft = 0;
        break;
      case OperandLifeTime::CONSTANT_REFERENCE: {
        auto poolIndex = from.location().poolindex();
        nnAssert(poolIndex < mModelPoolInfos.size());
        auto& r = mModelPoolInfos[poolIndex];
        to.buffer = r.buffer + from.location().offset();
        to.numberOfUsesLeft = 0;
        break;
      }
      case OperandLifeTime::MODEL_INPUT:
      case OperandLifeTime::MODEL_OUTPUT:
      case OperandLifeTime::NO_VALUE:
        to.buffer = nullptr;
        to.numberOfUsesLeft = 0;
        break;
      default:
        nnAssert(false);
        break;
    }
  }

  // Adjust the runtime info for the arguments passed to the model,
  // modifying the buffer location, and possibly the dimensions.
  auto updateForArguments = [&](const std::vector<uint32_t>& indexes,
                                const std::vector<RequestArgument>& arguments) {
    nnAssert(indexes.size() == arguments.size());
    for (size_t i = 0; i < indexes.size(); i++) {
      const uint32_t operandIndex = indexes[i];
      const RequestArgument& from = arguments[i];
      RunTimeOperandInfo& to = operands->at(operandIndex);
      if (from.dimensions().size() > 0) {
        // It's the responsibility of the caller to validate that
        // from.dimensions only modifies the dimensions that were
        // unspecified in the model.  That's the case in SampleDriver.cpp
        // with the call to validateRequest().
        // TODO make sure that's the case for the default CPU path.
        to.dimensions = {from.dimensions().begin(), from.dimensions().end()};
      }
      if (from.hasnovalue()) {
        to.lifetime = OperandLifeTime::NO_VALUE;
        nnAssert(to.buffer == nullptr);
      } else {
        auto poolIndex = from.location().poolindex();
        nnAssert(poolIndex < requestPoolInfos.size());
        auto& r = requestPoolInfos[poolIndex];
        to.buffer = r.buffer + from.location().offset();
      }
    }
  };
  updateForArguments(
      {mModel->inputindexes().begin(), mModel->inputindexes().end()},
      {request.inputs().begin(), request.inputs().end()});
  updateForArguments(
      {mModel->outputindexes().begin(), mModel->outputindexes().end()},
      {request.outputs().begin(), request.outputs().end()});

  return true;
}

void OemExecutor::freeNoLongerUsedOperands(
    const std::vector<uint32_t>& inputs,
    std::vector<RunTimeOperandInfo>* operands) {
  for (uint32_t i : inputs) {
    auto& info = operands->at(i);
    // Check if it's a static or model input/output.
    if (info.numberOfUsesLeft == 0) {
      continue;
    }
    info.numberOfUsesLeft--;
    if (info.numberOfUsesLeft == 0) {
      nnAssert(info.buffer != nullptr);
      delete[] info.buffer;
      info.buffer = nullptr;
    }
  }
}

int OemExecutor::executeOperation(const Operation& operation,
                                  std::vector<RunTimeOperandInfo>* operands) {
  int32_t oemModel = operation.oemmodel();
  LOG(INFO) << "execute OEM model #" << oemModel;
  ResultCode res = ANEURALNETWORKS_NO_ERROR;
  switch (static_cast<paintbox_nn::OemModel>(oemModel)) {
    case paintbox_nn::OemModel::MATRIX_ADD: {
      MatrixAddEngine engine;
      res = engine.run(operation, operands);
      break;
    }
    case paintbox_nn::OemModel::MOBILE_NET_BODY: {
      MobileNetBodyEngine engine;
      res = engine.run(operation, operands);
      break;
    }
    default: {
      LOG(ERROR) << "OemModel #" << oemModel << " not supported";
      return ANEURALNETWORKS_BAD_DATA;
    }
  }

  freeNoLongerUsedOperands(
      {operation.inputs().begin(), operation.inputs().end()}, operands);
  return res;
}

}  // namespace paintbox_nn
