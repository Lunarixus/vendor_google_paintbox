#define LOG_TAG "PAINTBOX_NN_CONVERSION"

#include "Conversion.h"
#include "android-base/logging.h"
#include "log/log.h"

namespace paintbox_util {
namespace {

paintbox_nn::OperandType convertHidlOperandType(const OperandType type) {
  switch (type) {
    case OperandType::FLOAT16:
      return paintbox_nn::FLOAT16;
    case OperandType::FLOAT32:
      return paintbox_nn::FLOAT32;
    case OperandType::INT8:
      return paintbox_nn::INT8;
    case OperandType::UINT8:
      return paintbox_nn::UINT8;
    case OperandType::INT16:
      return paintbox_nn::INT16;
    case OperandType::INT32:
      return paintbox_nn::INT32;
    case OperandType::UINT32:
      return paintbox_nn::UINT32;
    case OperandType::TENSOR_FLOAT16:
      return paintbox_nn::TENSOR_FLOAT16;
    case OperandType::TENSOR_FLOAT32:
      return paintbox_nn::TENSOR_FLOAT32;
    case OperandType::TENSOR_QUANT8_ASYMM:
      return paintbox_nn::TENSOR_QUANT8_ASYMM;
    default:
      CHECK(false) << "invalid operand type.";
      return paintbox_nn::FLOAT16;
  }
}

paintbox_nn::OperationType convertHidlOperationType(const OperationType type) {
  switch (type) {
    case OperationType::AVERAGE_POOL:
      return paintbox_nn::AVERAGE_POOL;
    case OperationType::CONCATENATION:
      return paintbox_nn::CONCATENATION;
    case OperationType::CONV:
      return paintbox_nn::CONV;
    case OperationType::DEPTHWISE_CONV:
      return paintbox_nn::DEPTHWISE_CONV;
    case OperationType::MAX_POOL:
      return paintbox_nn::MAX_POOL;
    case OperationType::L2_POOL:
      return paintbox_nn::L2_POOL;
    case OperationType::DEPTH_TO_SPACE:
      return paintbox_nn::DEPTH_TO_SPACE;
    case OperationType::SPACE_TO_DEPTH:
      return paintbox_nn::SPACE_TO_DEPTH;
    case OperationType::LOCAL_RESPONSE_NORMALIZATION:
      return paintbox_nn::LOCAL_RESPONSE_NORMALIZATION;
    case OperationType::SOFTMAX:
      return paintbox_nn::SOFTMAX;
    case OperationType::RESHAPE:
      return paintbox_nn::RESHAPE;
    case OperationType::SPLIT:
      return paintbox_nn::SPLIT;
    case OperationType::FAKE_QUANT:
      return paintbox_nn::FAKE_QUANT;
    case OperationType::ADD:
      return paintbox_nn::ADD;
    case OperationType::FULLY_CONNECTED:
      return paintbox_nn::FULLY_CONNECTED;
    case OperationType::CAST:
      return paintbox_nn::CAST;
    case OperationType::MUL:
      return paintbox_nn::MUL;
    case OperationType::L2_NORMALIZATION:
      return paintbox_nn::L2_NORMALIZATION;
    case OperationType::LOGISTIC:
      return paintbox_nn::LOGISTIC;
    case OperationType::RELU:
      return paintbox_nn::RELU;
    case OperationType::RELU6:
      return paintbox_nn::RELU6;
    case OperationType::RELU1:
      return paintbox_nn::RELU1;
    case OperationType::TANH:
      return paintbox_nn::TANH;
    case OperationType::DEQUANTIZE:
      return paintbox_nn::DEQUANTIZE;
    case OperationType::FLOOR:
      return paintbox_nn::FLOOR;
    case OperationType::GATHER:
      return paintbox_nn::GATHER;
    case OperationType::RESIZE_BILINEAR:
      return paintbox_nn::RESIZE_BILINEAR;
    case OperationType::LSH_PROJECTION:
      return paintbox_nn::LSH_PROJECTION;
    case OperationType::LSTM:
      return paintbox_nn::LSTM;
    case OperationType::SVDF:
      return paintbox_nn::SVDF;
    case OperationType::RNN:
      return paintbox_nn::RNN;
    case OperationType::N_GRAM:
      return paintbox_nn::N_GRAM;
    case OperationType::LOOKUP:
      return paintbox_nn::LOOKUP;
    default:
      CHECK(false) << "invalid operation type.";
      return paintbox_nn::AVERAGE_POOL;
  }
}

}  // namespace

void convertHidlModel(const Model& inputModel,
                      paintbox_nn::Model* outputModel) {
  for (auto operand : inputModel.operands) {
    auto protoOperand = outputModel->add_operands();
    protoOperand->set_type(convertHidlOperandType(operand.type));
    for (auto dimension : operand.dimensions) {
      protoOperand->add_dimensions(dimension);
    }
    protoOperand->set_numberofconsumers(operand.numberOfConsumers);
    protoOperand->set_scale(operand.scale);
    protoOperand->set_zeropoint(operand.zeroPoint);

    paintbox_nn::DataLocation* location = new paintbox_nn::DataLocation();
    location->set_poolindex(operand.location.poolIndex);
    location->set_offset(operand.location.offset);
    location->set_length(operand.location.length);
    protoOperand->set_allocated_location(location);
  }

  for (auto operation : inputModel.operations) {
    auto protoOperation = outputModel->add_operations();
    paintbox_nn::OperationTuple* tuple = new paintbox_nn::OperationTuple();
    tuple->set_operationtype(
        convertHidlOperationType(operation.opTuple.operationType));
    tuple->set_operandtype(
        convertHidlOperandType(operation.opTuple.operandType));
    protoOperation->set_allocated_optuple(tuple);

    for (auto input : operation.inputs) {
      protoOperation->add_inputs(input);
    }

    for (auto output : operation.outputs) {
      protoOperation->add_outputs(output);
    }
  }

  for (auto inputIndex : inputModel.inputIndexes) {
    outputModel->add_inputindexes(inputIndex);
  }
  for (auto outputIndex : inputModel.outputIndexes) {
    outputModel->add_outputindexes(outputIndex);
  }
  outputModel->set_operandvalues(inputModel.operandValues.data(),
                                 inputModel.operandValues.size());
}

void convertHidlRequest(const Request& inputRequest,
                        paintbox_nn::Request* outputRequest) {
  for (auto input : inputRequest.inputs) {
    auto protoInput = outputRequest->add_inputs();
    paintbox_nn::DataLocation* location = new paintbox_nn::DataLocation();
    location->set_poolindex(input.location.poolIndex);
    location->set_offset(input.location.offset);
    location->set_length(input.location.length);
    protoInput->set_allocated_location(location);

    for (auto dimension : input.dimensions) {
      protoInput->add_dimensions(dimension);
    }
  }

  for (auto output : inputRequest.outputs) {
    auto protoOutput = outputRequest->add_outputs();
    paintbox_nn::DataLocation* location = new paintbox_nn::DataLocation();
    location->set_poolindex(output.location.poolIndex);
    location->set_offset(output.location.offset);
    location->set_length(output.location.length);
    protoOutput->set_allocated_location(location);

    for (auto dimension : output.dimensions) {
      protoOutput->add_dimensions(dimension);
    }
  }

  for (auto& pool : inputRequest.pools) {
    outputRequest->add_poolsizes(pool.size());
  }
}

}  // namespace paintbox_util
