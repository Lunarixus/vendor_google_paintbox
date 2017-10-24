#ifndef PAINTBOX_NN_CONVERSION_H
#define PAINTBOX_NN_CONVERSION_H

#include "EaselComm2.h"
#include "HalInterfaces.h"
#include "vendor/google_paintbox/nn/shared/proto/types.pb.h"

namespace paintbox_util {

// Converts HIDL Model to protobuffer Model type.
void convertHidlModel(const Model& inputModel, paintbox_nn::Model* outputModel);

// Converts HIDL Request to protobuffer Request type.
void convertHidlRequest(const Request& inputModel,
                        paintbox_nn::Request* outputRequest);

// Converts HIDL memory to EaselComm2 hardware buffer.
// Returns true if convertion is successful, otherwise false.
bool mapPool(const hidl_memory& pool,
             EaselComm2::HardwareBuffer* hardwareBuffer);

// Converts the protobuffer error code to Android NN error code.
ErrorStatus convertProtoError(paintbox_nn::ErrorStatus error);

}  // namespace paintbox_util

#endif  // PAINTBOX_NN_CONVERSION_H
