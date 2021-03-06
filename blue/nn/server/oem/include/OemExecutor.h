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

#ifndef PAINTBOX_NN_OEM_EXECUTOR_H
#define PAINTBOX_NN_OEM_EXECUTOR_H

#include "HalInterfaces.h"
#include "OperationsUtils.h"
#include "Utils.h"

#include <algorithm>
#include <vector>

namespace paintbox_nn {

using android::nn::Shape;
using android::nn::sizeOfData;

// Information we maintain about each operand during execution that
// may change during execution.
struct RunTimeOperandInfo {
  // TODO Storing the type here is redundant, as it won't change during
  // execution.
  OperandType type;
  // The type and dimensions of the operand.  The dimensions can
  // change at runtime.  We include the type because it's useful
  // to pass together with the dimension to the functions implementing
  // the operators.
  std::vector<uint32_t> dimensions;

  float scale;
  int32_t zeroPoint;
  // Where the operand's data is stored.  Check the corresponding
  // location information in the model to figure out if this points
  // to memory we have allocated for an temporary operand.
  uint8_t* buffer;
  // The length of the buffer.
  uint32_t length;
  // Whether this is a temporary variable, a model input, a constant, etc.
  OperandLifeTime lifetime;
  // Keeps track of how many operations have yet to make use
  // of this temporary variable.  When the count is decremented to 0,
  // we free the buffer.  For non-temporary variables, this count is
  // always 0.
  uint32_t numberOfUsesLeft;

  Shape shape() const {
    return Shape{.type = type,
                 .dimensions = dimensions,
                 .scale = scale,
                 .offset = zeroPoint};
  }

  bool setInfoAndAllocateIfNeeded(const Shape& shape);
};

// Used to keep a pointer and size to each of the memory pools.
struct RunTimePoolInfo {
  uint8_t* buffer;
  size_t size;
};

// This class is used to execute an model with OEM operations.
class OemExecutor {
 public:
  // Ownership of model is transferred to OemExecutor.
  explicit OemExecutor(std::unique_ptr<Model> model);
  ~OemExecutor();

  // Allocates the RunTimePoolInfo for pool at index.
  // Returns the valid RunTimePoolInfo pointer if successful, otherwise nullptr.
  RunTimePoolInfo* allocModelPoolInfo(size_t index);

  // Returns true if all the pools for the model are valid, otherwise false.
  bool ready();

  // Executes the model. The results will be stored at requestPoolInfos.
  // Returns ANEURALNETWORKS_NO_ERROR if successful, otherwise the error code.
  int run(const Request& request,
          const std::vector<RunTimePoolInfo>& requestPoolInfos);

 private:
  // Initializes the RunTimeOperandInfo operands.
  // Returns true if successful, otherwise false.
  bool initializeRunTimeInfo(
      const Request& request,
      const std::vector<RunTimePoolInfo>& requestPoolInfos,
      std::vector<RunTimeOperandInfo>* operands);
  // Runs one operation of the graph.
  int executeOperation(const Operation& entry,
                       std::vector<RunTimeOperandInfo>* operands);
  // Decrement the usage count for the operands listed.  Frees the memory
  // allocated for any temporary variable with a count of zero.
  void freeNoLongerUsedOperands(const std::vector<uint32_t>& inputs,
                                std::vector<RunTimeOperandInfo>* operands);

  // The model and the pools that we'll use in execute.
  std::unique_ptr<Model> mModel;
  std::vector<RunTimePoolInfo> mModelPoolInfos;
};

namespace {

template <typename T>
T getScalarData(const RunTimeOperandInfo& info) {
  // TODO: Check buffer is at least as long as size of data.
  T* data = reinterpret_cast<T*>(info.buffer);
  return data[0];
}

}  // anonymous namespace

}  // namespace paintbox_nn

#endif  // PAINTBOX_NN_OEM_EXECUTOR_H
