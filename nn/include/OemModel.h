#ifndef PAINTBOX_NN_OEM_MODEL_H
#define PAINTBOX_NN_OEM_MODEL_H

namespace paintbox_nn {

// Supported OEM Model types.
// This must be specified as the first input operand value through
// setOperandValue.
// Example: android::nn::wrapper::Model* model;
// android::nn::wrapper::OperandType oemModelType(Type::INT32, {});
// int32_t oemModel = static_cast<int32_t>(paintbox_nn::MATRIX_ADD);
// auto select = model->addOperand(&oemModelType);
// model->setOperandValue(select, &oemModel, sizeof(oemModel));
// (adding other operations and operands...)
// model->finish();
enum class OemModel : int32_t {
  // Default index, not supported.
  UNKNOWN_OEM_MDOEL = -1,
  // Reserve for supported OemModel in production
  PROD_MODEL_PLACEHOLDER = 0,
  // Test OEM models starts from here.
  MATRIX_ADD = 10000,
};

}  // namespace paintbox_nn

#endif  // PAINTBOX_NN_OEM_MODEL_H
