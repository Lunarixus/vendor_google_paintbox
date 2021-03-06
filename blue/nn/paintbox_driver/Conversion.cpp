#define LOG_TAG "PAINTBOX_NN_CONVERSION"

#include "Conversion.h"
#include "Utils.h"
#include "android-base/logging.h"
#include "hardware/gchips/paintbox/system/include/easel_comm_helper.h"

#include <sys/mman.h>
#include <set>

namespace paintbox_util {

using android::sp;
using android::nn::getSizeFromInts;

namespace {

paintbox_nn::OperandType convertHidlOperandType(OperandType type) {
  switch (type) {
    case OperandType::FLOAT32:
      return paintbox_nn::FLOAT32;
    case OperandType::INT32:
      return paintbox_nn::INT32;
    case OperandType::UINT32:
      return paintbox_nn::UINT32;
    case OperandType::TENSOR_FLOAT32:
      return paintbox_nn::TENSOR_FLOAT32;
    case OperandType::TENSOR_QUANT8_ASYMM:
      return paintbox_nn::TENSOR_QUANT8_ASYMM;
    case OperandType::OEM:
      return paintbox_nn::OEM;
    case OperandType::TENSOR_OEM_BYTE:
      return paintbox_nn::TENSOR_OEM_BYTE;
    default:
      CHECK(false) << "invalid operand type.";
      return paintbox_nn::FLOAT32;
  }
}

paintbox_nn::OperandLifeTime convertHidlOperandLifeTime(OperandLifeTime type) {
  switch (type) {
    case OperandLifeTime::TEMPORARY_VARIABLE:
      return paintbox_nn::TEMPORARY_VARIABLE;
    case OperandLifeTime::MODEL_INPUT:
      return paintbox_nn::MODEL_INPUT;
    case OperandLifeTime::MODEL_OUTPUT:
      return paintbox_nn::MODEL_OUTPUT;
    case OperandLifeTime::CONSTANT_COPY:
      return paintbox_nn::CONSTANT_COPY;
    case OperandLifeTime::CONSTANT_REFERENCE:
      return paintbox_nn::CONSTANT_REFERENCE;
    case OperandLifeTime::NO_VALUE:
      return paintbox_nn::NO_VALUE;
    default:
      CHECK(false) << "invalid operand life time.";
      return paintbox_nn::TEMPORARY_VARIABLE;
  }
}

}  // namespace

paintbox_nn::OemModel getOemModel(const Model& model,
                                  const Operation& operation) {
  if (operation.type != OperationType::OEM_OPERATION) {
    return paintbox_nn::OemModel::UNKNOWN_OEM_MDOEL;
  }
  CHECK(operation.inputs.size() >= 1);
  const Operand& firstOperand = model.operands[operation.inputs[0]];
  if (firstOperand.type != OperandType::INT32) {
    return paintbox_nn::OemModel::UNKNOWN_OEM_MDOEL;
  }
  uint32_t offset = firstOperand.location.offset;
  int32_t oemModel =
      *(reinterpret_cast<const int32_t*>(model.operandValues.data() + offset));
  return static_cast<paintbox_nn::OemModel>(oemModel);
}

void convertHidlModel(const Model& inputModel, int64_t modelId,
                      paintbox_nn::Model* outputModel) {
  for (auto& operand : inputModel.operands) {
    auto protoOperand = outputModel->add_operands();
    protoOperand->set_type(convertHidlOperandType(operand.type));
    for (auto dimension : operand.dimensions) {
      protoOperand->add_dimensions(dimension);
    }
    protoOperand->set_numberofconsumers(operand.numberOfConsumers);
    protoOperand->set_scale(operand.scale);
    protoOperand->set_zeropoint(operand.zeroPoint);
    protoOperand->set_lifetime(convertHidlOperandLifeTime(operand.lifetime));

    paintbox_nn::DataLocation* location = new paintbox_nn::DataLocation();
    location->set_poolindex(operand.location.poolIndex);
    location->set_offset(operand.location.offset);
    location->set_length(operand.location.length);
    protoOperand->set_allocated_location(location);
  }

  for (auto operation : inputModel.operations) {
    auto protoOperation = outputModel->add_operations();
    // i starts from 1 so we skip the oem model index in input.
    for (size_t i = 1; i < operation.inputs.size(); i++) {
      protoOperation->add_inputs(operation.inputs[i]);
    }

    for (size_t i = 0; i < operation.outputs.size(); i++) {
      protoOperation->add_outputs(operation.outputs[i]);
    }

    protoOperation->set_oemmodel(
        static_cast<int32_t>(getOemModel(inputModel, operation)));
  }

  for (auto inputIndex : inputModel.inputIndexes) {
    outputModel->add_inputindexes(inputIndex);
  }
  for (auto outputIndex : inputModel.outputIndexes) {
    outputModel->add_outputindexes(outputIndex);
  }
  outputModel->set_operandvalues(inputModel.operandValues.data(),
                                 inputModel.operandValues.size());

  for (auto& pool : inputModel.pools) {
    outputModel->add_poolsizes(pool.size());
  }

  outputModel->set_modelid(modelId);
}

void convertHidlRequest(const Request& inputRequest, int64_t modelId,
                        paintbox_nn::Request* outputRequest) {
  std::set<uint32_t> inputPoolSet;
  for (auto& input : inputRequest.inputs) {
    auto protoInput = outputRequest->add_inputs();

    protoInput->set_hasnovalue(input.hasNoValue);

    paintbox_nn::DataLocation* location = new paintbox_nn::DataLocation();
    location->set_poolindex(input.location.poolIndex);
    inputPoolSet.insert(input.location.poolIndex);
    location->set_offset(input.location.offset);
    location->set_length(input.location.length);
    protoInput->set_allocated_location(location);

    for (auto dimension : input.dimensions) {
      protoInput->add_dimensions(dimension);
    }
  }

  for (auto index : inputPoolSet) {
    outputRequest->add_inputpools(index);
  }

  std::set<uint32_t> outputPoolSet;
  for (auto& output : inputRequest.outputs) {
    auto protoOutput = outputRequest->add_outputs();

    protoOutput->set_hasnovalue(output.hasNoValue);

    paintbox_nn::DataLocation* location = new paintbox_nn::DataLocation();
    location->set_poolindex(output.location.poolIndex);
    outputPoolSet.insert(output.location.poolIndex);
    location->set_offset(output.location.offset);
    location->set_length(output.location.length);
    protoOutput->set_allocated_location(location);

    for (auto dimension : output.dimensions) {
      protoOutput->add_dimensions(dimension);
    }
  }

  for (auto index : outputPoolSet) {
    outputRequest->add_outputpools(index);
  }

  for (auto& pool : inputRequest.pools) {
    outputRequest->add_poolsizes(pool.size());
  }

  outputRequest->set_modelid(modelId);
}

// Reference: RunTimePoolInfo::set
// frameworks/ml/nn/common/CpuExecutor.cpp
// This function supports two hidl_memory types: ashmem and the mmap_fd.
// Ashmem hidl_memory will be unmapped in destructor.
// TODO(cjluo): consider unmmap flow for mmap_fd type.
bool mapPool(const hidl_memory& hidlMemory, HardwareBufferPool* bufferPool) {
  auto memType = hidlMemory.name();
  if (memType == "ashmem") {
    sp<IMemory> memory;
    memory = mapMemory(hidlMemory);
    if (memory == nullptr) {
      LOG(ERROR) << "Can't map shared memory.";
      return false;
    }
    memory->update();
    uint8_t* buffer =
        reinterpret_cast<uint8_t*>(static_cast<void*>(memory->getPointer()));
    if (buffer == nullptr) {
      LOG(ERROR) << "Can't access shared memory.";
      return false;
    }
    bufferPool->buffer = easel::CreateHardwareBuffer(buffer, memory->getSize());
    bufferPool->memory = memory;
    return true;
  } else if (memType == "mmap_fd") {
    size_t size = hidlMemory.size();
    int fd = hidlMemory.handle()->data[0];
    int prot = hidlMemory.handle()->data[1];
    size_t offset = getSizeFromInts(hidlMemory.handle()->data[2],
                                    hidlMemory.handle()->data[3]);
    uint8_t* buffer = static_cast<uint8_t*>(
        mmap(nullptr, size, prot, MAP_SHARED, fd, offset));
    if (buffer == MAP_FAILED) {
      LOG(ERROR) << "Can't mmap the file descriptor.";
      return false;
    }
    bufferPool->buffer = easel::CreateHardwareBuffer(buffer, size);
    bufferPool->memory = nullptr;
  } else {
    LOG(ERROR) << "unsupported hidl_memory type";
    return false;
  }
  return false;
}

ErrorStatus convertProtoError(paintbox_nn::ErrorStatus error) {
  switch (error) {
    case paintbox_nn::NONE:
      return ErrorStatus::NONE;
    case paintbox_nn::DEVICE_UNAVAILABLE:
      return ErrorStatus::DEVICE_UNAVAILABLE;
    case paintbox_nn::GENERAL_FAILURE:
      return ErrorStatus::GENERAL_FAILURE;
    case paintbox_nn::OUTPUT_INSUFFICIENT_SIZE:
      return ErrorStatus::OUTPUT_INSUFFICIENT_SIZE;
    case paintbox_nn::INVALID_ARGUMENT:
      return ErrorStatus::INVALID_ARGUMENT;
    default:
      return ErrorStatus::GENERAL_FAILURE;
  }
}

}  // namespace paintbox_util
