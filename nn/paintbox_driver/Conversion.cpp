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
    case OperandType::TENSOR_SYMMETRICAL_QUANT8:
      return paintbox_nn::TENSOR_SYMMETRICAL_QUANT8;
    default:
      CHECK(false) << "invalid operand type.";
      return paintbox_nn::FLOAT16;
  }
}

paintbox_nn::OperationType convertHidlOperationType(const OperationType type) {
  switch (type) {
    case OperationType::AVERAGE_POOL_FLOAT32:
      return paintbox_nn::AVERAGE_POOL_FLOAT32;
    case OperationType::CONCATENATION_FLOAT32:
      return paintbox_nn::CONCATENATION_FLOAT32;
    case OperationType::CONV_FLOAT32:
      return paintbox_nn::CONV_FLOAT32;
    case OperationType::DEPTHWISE_CONV_FLOAT32:
      return paintbox_nn::DEPTHWISE_CONV_FLOAT32;
    case OperationType::MAX_POOL_FLOAT32:
      return paintbox_nn::MAX_POOL_FLOAT32;
    case OperationType::L2_POOL_FLOAT32:
      return paintbox_nn::L2_POOL_FLOAT32;
    case OperationType::DEPTH_TO_SPACE_FLOAT32:
      return paintbox_nn::DEPTH_TO_SPACE_FLOAT32;
    case OperationType::SPACE_TO_DEPTH_FLOAT32:
      return paintbox_nn::SPACE_TO_DEPTH_FLOAT32;
    case OperationType::LOCAL_RESPONSE_NORMALIZATION_FLOAT32:
      return paintbox_nn::LOCAL_RESPONSE_NORMALIZATION_FLOAT32;
    case OperationType::SOFTMAX_FLOAT32:
      return paintbox_nn::SOFTMAX_FLOAT32;
    case OperationType::RESHAPE_FLOAT32:
      return paintbox_nn::RESHAPE_FLOAT32;
    case OperationType::SPLIT_FLOAT32:
      return paintbox_nn::SPLIT_FLOAT32;
    case OperationType::FAKE_QUANT_FLOAT32:
      return paintbox_nn::FAKE_QUANT_FLOAT32;
    case OperationType::ADD_FLOAT32:
      return paintbox_nn::ADD_FLOAT32;
    case OperationType::FULLY_CONNECTED_FLOAT32:
      return paintbox_nn::FULLY_CONNECTED_FLOAT32;
    case OperationType::CAST_FLOAT32:
      return paintbox_nn::CAST_FLOAT32;
    case OperationType::MUL_FLOAT32:
      return paintbox_nn::MUL_FLOAT32;
    case OperationType::L2_NORMALIZATION_FLOAT32:
      return paintbox_nn::L2_NORMALIZATION_FLOAT32;
    case OperationType::LOGISTIC_FLOAT32:
      return paintbox_nn::LOGISTIC_FLOAT32;
    case OperationType::RELU_FLOAT32:
      return paintbox_nn::RELU_FLOAT32;
    case OperationType::RELU6_FLOAT32:
      return paintbox_nn::RELU6_FLOAT32;
    case OperationType::RELU1_FLOAT32:
      return paintbox_nn::RELU1_FLOAT32;
    case OperationType::TANH_FLOAT32:
      return paintbox_nn::TANH_FLOAT32;
    case OperationType::DEQUANTIZE_FLOAT32:
      return paintbox_nn::DEQUANTIZE_FLOAT32;
    case OperationType::FLOOR_FLOAT32:
      return paintbox_nn::FLOOR_FLOAT32;
    case OperationType::GATHER_FLOAT32:
      return paintbox_nn::GATHER_FLOAT32;
    case OperationType::RESIZE_BILINEAR_FLOAT32:
      return paintbox_nn::RESIZE_BILINEAR_FLOAT32;
    case OperationType::LSH_PROJECTION_FLOAT32:
      return paintbox_nn::LSH_PROJECTION_FLOAT32;
    case OperationType::LSTM_FLOAT32:
      return paintbox_nn::LSTM_FLOAT32;
    case OperationType::SVDF_FLOAT32:
      return paintbox_nn::SVDF_FLOAT32;
    case OperationType::RNN_FLOAT32:
      return paintbox_nn::RNN_FLOAT32;
    case OperationType::N_GRAM_FLOAT32:
      return paintbox_nn::N_GRAM_FLOAT32;
    case OperationType::LOOKUP_FLOAT32:
      return paintbox_nn::LOOKUP_FLOAT32;
    default:
      CHECK(false) << "invalid operation type.";
      return paintbox_nn::AVERAGE_POOL_FLOAT32;
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
    protoOperation->set_type(convertHidlOperationType(operation.type));

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
  for (auto operandValue : inputModel.operandValues) {
    outputModel->add_operandvalues(operandValue);
  }
  outputModel->set_poolssize(inputModel.pools.size());
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

  outputRequest->set_poolssize(inputRequest.pools.size());
}

}  // namespace paintbox_util