#ifndef PAINTBOX_NN_CONVERSION_H
#define PAINTBOX_NN_CONVERSION_H

#include "HalInterfaces.h"
#include "OemModel.h"
#include "hardware/gchips/paintbox/system/include/easel_comm.h"
#include "vendor/google_paintbox/blue/nn/shared/proto/types.pb.h"

namespace paintbox_util {

// Gets the OemModel type from OEM operation.
// Returns found OemModel, otherwise UNKNOWN_OEM_MDOEL.
paintbox_nn::OemModel getOemModel(const Model& model,
                                  const Operation& operation);

// Converts HIDL Model to protobuffer Model type.
// The OEM model type input will be ignored.
void convertHidlModel(const Model& inputModel, paintbox_nn::Model* outputModel);

// Converts HIDL Request to protobuffer Request type.
void convertHidlRequest(const Request& inputModel,
                        paintbox_nn::Request* outputRequest);

// The structure maintaining hardwarebuffer together with the mmapped memory
// handle. memory needs to be keep alive during the time when mmapped virtual
// address is needed.
struct HardwareBufferPool {
  std::unique_ptr<easel::HardwareBuffer> buffer;
  android::sp<IMemory> memory;
};

// Converts HIDL memory to HardwareBufferPool.
// Returns true if conversion is successful, otherwise false.
bool mapPool(const hidl_memory& pool, HardwareBufferPool* bufferPool);

// Converts the protobuffer error code to Android NN error code.
ErrorStatus convertProtoError(paintbox_nn::ErrorStatus error);

}  // namespace paintbox_util

#endif  // PAINTBOX_NN_CONVERSION_H
