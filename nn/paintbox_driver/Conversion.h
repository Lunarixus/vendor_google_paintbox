#ifndef PAINTBOX_NN_CONVERSION_H
#define PAINTBOX_NN_CONVERSION_H

#include "HalInterfaces.h"
#include "vendor/google_paintbox/nn/shared/proto/types.pb.h"

namespace paintbox_util {

// Converts HIDL Model to protobuffer Model type.
void convertHidlModel(const Model& inputModel, paintbox_nn::Model* outputModel);

// Converts HIDL Request to protobuffer Request type.
void convertHidlRequest(const Request& inputModel,
                        paintbox_nn::Request* outputRequest);

}  // namespace paintbox_util

#endif  // PAINTBOX_NN_CONVERSION_H
