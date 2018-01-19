#include "OemOperationEngine.h"

namespace paintbox_nn {

OemOperationEngine::OemOperationEngine() {}
OemOperationEngine::~OemOperationEngine() {}
ResultCode OemOperationEngine::run(const Operation& operation,
                                   std::vector<RunTimeOperandInfo>* operands) {
  if (!verify(operation, *operands)) return ANEURALNETWORKS_BAD_DATA;
  return execute(operation, operands);
}

}  // namespace paintbox_nn