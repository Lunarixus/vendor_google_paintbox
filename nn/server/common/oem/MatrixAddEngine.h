#ifndef ANDROID_ML_NN_COMMON_OEM_MATRIX_ADD_ENGINE_H
#define ANDROID_ML_NN_COMMON_OEM_MATRIX_ADD_ENGINE_H

#include "OemOperationEngine.h"

namespace android {
namespace nn {

// OemModel for a simple MatrixAdd.
// Input: matrix a and b as TENSOR_FLOAT32. The third input is activation.
// Output: matrix c = a + b.
class MatrixAddEngine : public OemOperationEngine {
 public:
  MatrixAddEngine();
  ~MatrixAddEngine();

 protected:
  bool verify(const Operation& operation,
              const std::vector<RunTimeOperandInfo>& operands) override;
  ResultCode execute(const Operation& operation,
                     std::vector<RunTimeOperandInfo>* operands) override;
};

}  // namespace nn
}  // namespace android

#endif  // ANDROID_ML_NN_COMMON_OEM_MATRIX_ADD_ENGINE_H
