#define LOG_TAG "MobileNetBodyEngine"

#include "MobileNetBodyEngine.h"

#include "log/log.h"

#include "ActivationFunctor.h"
#include "Operations.h"
#include "OperationsUtils.h"

namespace paintbox_nn {

using android::nn::calculateExplicitPadding;
using android::nn::kPaddingSame;

namespace {
const int kNumLayers = 27;

// Calculates paddings (left, right, top, bottom).
// Returns true when successful, otherwise false.
bool calculatePadding(const Shape& inputShape, int32_t stride,
                      const Shape& filterShape, int32_t paddingImplicit,
                      std::vector<int32_t>* paddings) {
  if (paddings == nullptr) return false;

  paddings->resize(4);
  int32_t inputWidth = getSizeOfDimension(inputShape, 2);
  int32_t inputHeight = getSizeOfDimension(inputShape, 1);
  int32_t filterWidth = getSizeOfDimension(filterShape, 2);
  int32_t filterHeight = getSizeOfDimension(filterShape, 1);
  calculateExplicitPadding(inputWidth, stride, filterWidth, paddingImplicit,
                           paddings->data(), paddings->data() + 1);
  calculateExplicitPadding(inputHeight, stride, filterHeight, paddingImplicit,
                           paddings->data() + 2, paddings->data() + 3);
  return true;
}
}  // namespace

MobileNetBodyEngine::MobileNetBodyEngine() {}
MobileNetBodyEngine::~MobileNetBodyEngine() {}

bool MobileNetBodyEngine::verify(const Operation& operation,
                                 const std::vector<RunTimeOperandInfo>&) {
  bool success = (operation.inputs().size() == 1 + kNumLayers * 2) &&
                 (operation.outputs().size() == 1);

  // TOOD(cjluo): verify the operands too.
  return success;
}

ResultCode MobileNetBodyEngine::execute(
    const Operation& operation, std::vector<RunTimeOperandInfo>* operands) {
  const RunTimeOperandInfo& input = operands->at(operation.inputs(0));
  const float* filterBuffer[kNumLayers];
  const float* biasBuffer[kNumLayers];
  Shape filterShape[kNumLayers];
  Shape biasShape[kNumLayers];
  for (int i = 0; i < kNumLayers; i++) {
    RunTimeOperandInfo& filter = operands->at(operation.inputs(i * 2 + 1));
    filterShape[i] = filter.shape();
    filterBuffer[i] = reinterpret_cast<const float*>(filter.buffer);
    RunTimeOperandInfo& bias = operands->at(operation.inputs(i * 2 + 2));
    biasShape[i] = bias.shape();
    biasBuffer[i] = reinterpret_cast<const float*>(bias.buffer);
  }

  const int32_t padding = static_cast<int32_t>(kPaddingSame);
  const int32_t depthMultiplier = 1;
  const int32_t activation = static_cast<int32_t>(kActivationRelu6);

  const int32_t stride[kNumLayers] = {
      2, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 2, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1,
  };
  RunTimeOperandInfo& output = operands->at(operation.outputs(0));
  Shape outShape = output.shape();
  Shape tmpInputShape;
  Shape tmpOutputShape;
  // TODO(cjluo): figure out why tmpInput and tmpOutput are static.
  static float tmpInput[112 * 112 * 64];
  static float tmpOutput[112 * 112 * 64];

  std::vector<int32_t> paddings = {};

  bool success = false;
  if (operands->at(operation.inputs(0)).type == OperandType::TENSOR_FLOAT32) {
    success = calculatePadding(input.shape(), stride[0], filterShape[0],
                               padding, &paddings) &&
              convPrepare(input.shape(), filterShape[0], biasShape[0],
                          paddings[0], paddings[1], paddings[2], paddings[3],
                          stride[0], stride[0], &tmpOutputShape) &&
              convFloat32(reinterpret_cast<const float*>(input.buffer),
                          input.shape(), filterBuffer[0], filterShape[0],
                          biasBuffer[0], biasShape[0], paddings[0], paddings[1],
                          paddings[2], paddings[3], stride[0], stride[0],
                          activation, tmpOutput, tmpOutputShape);
    for (int i = 1; i < kNumLayers - 2; i += 2) {
      success &=
          calculatePadding(tmpOutputShape, stride[i], filterShape[i], padding,
                           &paddings) &&
          depthwiseConvPrepare(tmpOutputShape, filterShape[i], biasShape[i],
                               paddings[0], paddings[1], paddings[2],
                               paddings[3], stride[i], stride[i],
                               &tmpInputShape) &&
          depthwiseConvFloat32(
              tmpOutput, tmpOutputShape, filterBuffer[i], filterShape[i],
              biasBuffer[i], biasShape[i], paddings[0], paddings[1],
              paddings[2], paddings[3], stride[i], stride[i], depthMultiplier,
              activation, tmpInput, tmpInputShape) &&
          calculatePadding(tmpInputShape, stride[i + 1], filterShape[i + 1],
                           padding, &paddings) &&
          convPrepare(tmpInputShape, filterShape[i + 1], biasShape[i + 1],
                      paddings[0], paddings[1], paddings[2], paddings[3],
                      stride[i + 1], stride[i + 1], &tmpOutputShape) &&
          convFloat32(tmpInput, tmpInputShape, filterBuffer[i + 1],
                      filterShape[i + 1], biasBuffer[i + 1], biasShape[i + 1],
                      paddings[0], paddings[1], paddings[2], paddings[3],
                      stride[i + 1], stride[i + 1], activation, tmpOutput,
                      tmpOutputShape);
    }
    success &=
        calculatePadding(tmpOutputShape, stride[kNumLayers - 2],
                         filterShape[kNumLayers - 2], padding, &paddings) &&
        depthwiseConvPrepare(tmpOutputShape, filterShape[kNumLayers - 2],
                             biasShape[kNumLayers - 2], paddings[0],
                             paddings[1], paddings[2], paddings[3],
                             stride[kNumLayers - 2], stride[kNumLayers - 2],
                             &tmpInputShape) &&
        depthwiseConvFloat32(
            tmpOutput, tmpOutputShape, filterBuffer[kNumLayers - 2],
            filterShape[kNumLayers - 2], biasBuffer[kNumLayers - 2],
            biasShape[kNumLayers - 2], paddings[0], paddings[1], paddings[2],
            paddings[3], stride[kNumLayers - 2], stride[kNumLayers - 2],
            depthMultiplier, activation, tmpInput, tmpInputShape) &&
        calculatePadding(tmpInputShape, stride[kNumLayers - 1],
                         filterShape[kNumLayers - 1], padding, &paddings) &&
        convPrepare(tmpInputShape, filterShape[kNumLayers - 1],
                    biasShape[kNumLayers - 1], paddings[0], paddings[1],
                    paddings[2], paddings[3], stride[kNumLayers - 1],
                    stride[kNumLayers - 1], &outShape) &&
        convFloat32(tmpInput, tmpInputShape, filterBuffer[kNumLayers - 1],
                    filterShape[kNumLayers - 1], biasBuffer[kNumLayers - 1],
                    biasShape[kNumLayers - 1], paddings[0], paddings[1],
                    paddings[2], paddings[3], stride[kNumLayers - 1],
                    stride[kNumLayers - 1], activation,
                    reinterpret_cast<float*>(output.buffer), outShape);
  }

  if (!success) {
    ALOGE("OEM_OPERATION failed.");
    return ANEURALNETWORKS_OP_FAILED;
  }
  return ANEURALNETWORKS_NO_ERROR;
}

}  // namespace paintbox_nn
