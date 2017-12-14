#ifndef ANDROID_ML_NN_COMMON_OEM_MOBILE_NET_BODY_ENGINE_H
#define ANDROID_ML_NN_COMMON_OEM_MOBILE_NET_BODY_ENGINE_H

#include "OemOperationEngine.h"

namespace android {
namespace nn {

// OemModel for a MobileNetBody.
// Input: 224 x 224 x 3 image.
// Output: 7 x 7 x 256 float.
class MobileNetBodyEngine : public OemOperationEngine {
 public:
  MobileNetBodyEngine();
  ~MobileNetBodyEngine();

 protected:
  bool verify(const Operation& operation,
              const std::vector<RunTimeOperandInfo>& operands) override;
  ResultCode execute(const Operation& operation,
                     std::vector<RunTimeOperandInfo>* operands) override;
};

}  // namespace nn
}  // namespace android

#endif  // ANDROID_ML_NN_COMMON_OEM_MOBILE_NET_BODY_ENGINE_H
