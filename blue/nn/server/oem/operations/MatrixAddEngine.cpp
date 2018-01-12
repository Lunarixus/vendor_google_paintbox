#define LOG_TAG "MatrixAddEngine"

#include "MatrixAddEngine.h"

#include "log/log.h"

#include "Operations.h"
#include "OperationsUtils.h"

namespace paintbox_nn {

MatrixAddEngine::MatrixAddEngine() {}
MatrixAddEngine::~MatrixAddEngine() {}
bool MatrixAddEngine::verify(const Operation& operation,
                             const std::vector<RunTimeOperandInfo>& operands) {
  bool success = (operation.inputs().size() == 3);
  success &= (operation.outputs().size() == 1);

  if (!success) return false;

  success &=
      (operands[operation.inputs(0)].lifetime == OperandLifeTime::MODEL_INPUT);
  success &=
      (operands[operation.inputs(1)].lifetime == OperandLifeTime::MODEL_INPUT);
  success &= (operands[operation.inputs(2)].lifetime ==
              OperandLifeTime::CONSTANT_COPY);
  success &= (operands[operation.outputs(0)].lifetime ==
              OperandLifeTime::MODEL_OUTPUT);

  Shape shape0 = operands[operation.inputs(0)].shape();
  Shape shape1 = operands[operation.inputs(1)].shape();
  Shape shape2 = operands[operation.outputs(0)].shape();
  success &= SameShape(shape0, shape1);
  success &= SameShape(shape0, shape2);

  return success;
}

ResultCode MatrixAddEngine::execute(const Operation& operation,
                                    std::vector<RunTimeOperandInfo>* operands) {
  const RunTimeOperandInfo& in1 = operands->at(operation.inputs(0));
  const RunTimeOperandInfo& in2 = operands->at(operation.inputs(1));
  int32_t activation =
      getScalarData<int32_t>(operands->at(operation.inputs(2)));

  RunTimeOperandInfo& out = operands->at(operation.outputs(0));
  Shape outShape = out.shape();

  bool success = false;
  if (in1.type == OperandType::TENSOR_FLOAT32) {
    success =
        addMulPrepare(in1.shape(), in2.shape(), &outShape) &&
        out.setInfoAndAllocateIfNeeded(outShape) &&
        addFloat32(reinterpret_cast<const float*>(in1.buffer), in1.shape(),
                   reinterpret_cast<const float*>(in2.buffer), in2.shape(),
                   activation, reinterpret_cast<float*>(out.buffer), outShape);
  } else if (in1.type == OperandType::TENSOR_QUANT8_ASYMM) {
    success =
        addMulPrepare(in1.shape(), in2.shape(), &outShape) &&
        out.setInfoAndAllocateIfNeeded(outShape) &&
        addQuant8(reinterpret_cast<const uint8_t*>(in1.buffer), in1.shape(),
                  reinterpret_cast<const uint8_t*>(in2.buffer), in2.shape(),
                  activation, reinterpret_cast<uint8_t*>(out.buffer), outShape);
  }

  if (!success) {
    ALOGE("OEM_OPERATION failed.");
    return ANEURALNETWORKS_OP_FAILED;
  }
  return ANEURALNETWORKS_NO_ERROR;
}

}  // namespace paintbox_nn
