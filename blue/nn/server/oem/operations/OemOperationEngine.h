#ifndef PAINTBOX_NN_OEM_OPERATIONS_OEM_OPERATION_ENGINE_H
#define PAINTBOX_NN_OEM_OPERATIONS_OEM_OPERATION_ENGINE_H

#include <vector>

#include "NeuralNetworks.h"
#include "OemExecutor.h"

namespace paintbox_nn {

// OemOperation Engine for processing OEM_OPERATION requests.
class OemOperationEngine {
 public:
  // Run the OEM_OPERATION and returns the ResultCode.
  ResultCode run(const Operation& operation,
                 std::vector<RunTimeOperandInfo>* operands);
  virtual ~OemOperationEngine();

 protected:
  OemOperationEngine();

  // Returns true if operation and operands are valid.
  virtual bool verify(const Operation& operation,
                      const std::vector<RunTimeOperandInfo>& operands) = 0;
  // Executes the operation with operands and returns the ResultCode.
  virtual ResultCode execute(const Operation& operation,
                             std::vector<RunTimeOperandInfo>* operands) = 0;
};

}  // namespace paintbox_nn

#endif  // PAINTBOX_NN_OEM_OPERATIONS_OEM_OPERATION_ENGINE_H
