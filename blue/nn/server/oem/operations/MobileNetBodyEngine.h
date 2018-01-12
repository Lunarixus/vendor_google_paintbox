#ifndef PAINTBOX_NN_OEM_OPERATIONS_MOBILE_NET_BODY_ENGINE_H
#define PAINTBOX_NN_OEM_OPERATIONS_MOBILE_NET_BODY_ENGINE_H

#include "OemOperationEngine.h"

namespace paintbox_nn {

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

}  // namespace paintbox_nn

#endif  // PAINTBOX_NN_OEM_OPERATIONS_MOBILE_NET_BODY_ENGINE_H
