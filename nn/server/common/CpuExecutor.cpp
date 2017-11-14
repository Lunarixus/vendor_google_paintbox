/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "CpuExecutor"

#include "CpuExecutor.h"

#include "NeuralNetworks.h"
#include "Operations.h"

#include "log/log.h"
#include <sys/mman.h>

namespace android {
namespace nn {

// Updates the RunTimeOperandInfo with the newly calculated shape.
// Allocate the buffer if we need to.
static bool setInfoAndAllocateIfNeeded(RunTimeOperandInfo* info, const Shape& shape) {
    // For user-provided model output operands, the parameters must match the Shape
    // calculated from the preparation step.
    if (info->lifetime == OperandLifeTime::MODEL_OUTPUT) {
        if (info->type != shape.type ||
            info->dimensions != shape.dimensions) {
            ALOGE("Invalid type or dimensions for model output");
            return false;
        }
        if (info->type == OperandType::TENSOR_QUANT8_ASYMM &&
            (info->scale != shape.scale || info->zeroPoint != shape.offset)) {
            ALOGE("Invalid scale or zeroPoint for model output");
            return false;
        }
    }
    info->type = shape.type;
    info->dimensions = shape.dimensions;
    info->scale = shape.scale;
    info->zeroPoint = shape.offset;
    if (info->lifetime == OperandLifeTime::TEMPORARY_VARIABLE && info->buffer == nullptr) {
        uint32_t length = sizeOfData(info->type, info->dimensions);
        info->buffer = new uint8_t[length];
        if (info->buffer == nullptr) {
            return false;
        }
    }
    return true;
}

// Ignore the .pools entry in model and request.  This will have been taken care of
// by the caller.
int CpuExecutor::run(const Model& model, const Request& request,
                     const std::vector<RunTimePoolInfo>& modelPoolInfos,
                     const std::vector<RunTimePoolInfo>& requestPoolInfos) {
    ALOGI("CpuExecutor::run()");

    mModel = &model;
    mRequest = &request; // TODO check if mRequest is needed
    initializeRunTimeInfo(modelPoolInfos, requestPoolInfos);
    // The model has serialized the operation in execution order.
    for (const auto& operation : model.operations()) {
        int n = executeOperation(operation);
        if (n != ANEURALNETWORKS_NO_ERROR) {
            return n;
        }
    }

    mModel = nullptr;
    mRequest = nullptr;
    ALOGI("Completed run normally");
    return ANEURALNETWORKS_NO_ERROR;
}

bool CpuExecutor::initializeRunTimeInfo(const std::vector<RunTimePoolInfo>& modelPoolInfos,
                                        const std::vector<RunTimePoolInfo>& requestPoolInfos) {
    ALOGI("CpuExecutor::initializeRunTimeInfo");
    const size_t count = mModel->operands().size();
    mOperands.resize(count);

    // Start by setting the runtime info to what's in the model.
    for (size_t i = 0; i < count; i++) {
        const Operand& from = mModel->operands(i);
        RunTimeOperandInfo& to = mOperands[i];
        to.type = from.type();
        to.dimensions = {from.dimensions().begin(), from.dimensions().end()};
        to.scale = from.scale();
        to.zeroPoint = from.zeropoint();
        to.length = from.location().length();
        to.lifetime = from.lifetime();
        switch (from.lifetime()) {
            case OperandLifeTime::TEMPORARY_VARIABLE:
                to.buffer = nullptr;
                to.numberOfUsesLeft = from.numberofconsumers();
                break;
            case OperandLifeTime::CONSTANT_COPY:
                to.buffer = reinterpret_cast<uint8_t*>(const_cast<char*>(mModel->operandvalues().c_str()) + from.location().offset());
                to.numberOfUsesLeft = 0;
                break;
            case OperandLifeTime::CONSTANT_REFERENCE: {
                auto poolIndex = from.location().poolindex();
                nnAssert(poolIndex < modelPoolInfos.size());
                auto& r = modelPoolInfos[poolIndex];
                to.buffer = r.buffer + from.location().offset();
                to.numberOfUsesLeft = 0;
                break;
            }
            case OperandLifeTime::MODEL_INPUT:
            case OperandLifeTime::MODEL_OUTPUT:
            case OperandLifeTime::NO_VALUE:
                to.buffer = nullptr;
                to.numberOfUsesLeft = 0;
                break;
            default:
                nnAssert(false);
                break;
        }
    }

    // Adjust the runtime info for the arguments passed to the model,
    // modifying the buffer location, and possibly the dimensions.
    auto updateForArguments = [this, &requestPoolInfos](const std::vector<uint32_t>& indexes,
                                  const std::vector<RequestArgument>& arguments) {
        nnAssert(indexes.size() == arguments.size());
        for (size_t i = 0; i < indexes.size(); i++) {
            const uint32_t operandIndex = indexes[i];
            const RequestArgument& from = arguments[i];
            RunTimeOperandInfo& to = mOperands[operandIndex];
            if (from.dimensions().size() > 0) {
                // It's the responsibility of the caller to validate that
                // from.dimensions only modifies the dimensions that were
                // unspecified in the model.  That's the case in SampleDriver.cpp
                // with the call to validateRequest().
                // TODO make sure that's the case for the default CPU path.
                to.dimensions = {from.dimensions().begin(), from.dimensions().end()};
            }
            if (from.hasnovalue()) {
                to.lifetime = OperandLifeTime::NO_VALUE;
                nnAssert(to.buffer == nullptr);
            } else {
                auto poolIndex = from.location().poolindex();
                nnAssert(poolIndex < requestPoolInfos.size());
                auto& r = requestPoolInfos[poolIndex];
                to.buffer = r.buffer + from.location().offset();
            }
        }
    };
    updateForArguments(
            {mModel->inputindexes().begin(), mModel->inputindexes().end()},
            {mRequest->inputs().begin(), mRequest->inputs().end()});
    updateForArguments(
            {mModel->outputindexes().begin(), mModel->outputindexes().end()},
            {mRequest->outputs().begin(), mRequest->outputs().end()});

    return true;
}

void CpuExecutor::freeNoLongerUsedOperands(const std::vector<uint32_t>& inputs) {
    for (uint32_t i : inputs) {
        auto& info = mOperands[i];
        // Check if it's a static or model input/output.
        if (info.numberOfUsesLeft == 0) {
            continue;
        }
        info.numberOfUsesLeft--;
        if (info.numberOfUsesLeft == 0) {
            nnAssert(info.buffer != nullptr);
            delete[] info.buffer;
            info.buffer = nullptr;
        }
    }
}

int CpuExecutor::executeOperation(const Operation& operation) {
    const std::vector<uint32_t>& ins = {operation.inputs().begin(), operation.inputs().end()};
    const std::vector<uint32_t>& outs = {operation.outputs().begin(), operation.outputs().end()};
    bool success = false;

    // Function to verify that the number of input and output parameters
    // matches what is expected.  Also checks that all the parameters have
    // values. This function is to be used only for operations that do not
    // accept optional arguments.
    // TODO Have a version that works for optional arguments.
    auto allParametersPresent = [&ins, &outs, this](size_t requiredIns,
                                                                size_t requiredOuts) -> bool {
        auto verify = [this](size_t requiredCount, const std::vector<uint32_t>& indexes,
                          const char* type) -> bool {
            size_t actualCount = indexes.size();
            if (actualCount != requiredCount) {
                LOG(ERROR) << ": Invalid number of " << type << " operands. Got " << actualCount
                           << " of " << requiredCount;
                return false;
            }
            for (size_t i = 0; i < actualCount; i++) {
                if (mOperands[indexes[i]].lifetime == OperandLifeTime::NO_VALUE) {
                    LOG(ERROR) << " " << type
                               << " operand " << i << " is required but missing.";
                    return false;
                }
            }
            return true;
        };
        return verify(requiredIns, ins, "in") && verify(requiredOuts, outs, "out");
    };

    // Assume OEM operation is Add for now
    if (!allParametersPresent(3, 1)) {
        return ANEURALNETWORKS_BAD_DATA;
    }
    const RunTimeOperandInfo& in1 = mOperands[ins[0]];
    const RunTimeOperandInfo& in2 = mOperands[ins[1]];
    int32_t activation = getScalarData<int32_t>(mOperands[ins[2]]);

    RunTimeOperandInfo& out = mOperands[outs[0]];
    Shape outShape = out.shape();

    if (in1.type == OperandType::TENSOR_FLOAT32) {
        success = addMulPrepare(in1.shape(), in2.shape(), &outShape) &&
                  setInfoAndAllocateIfNeeded(&out, outShape) &&
                  addFloat32(reinterpret_cast<const float*>(in1.buffer),
                             in1.shape(),
                             reinterpret_cast<const float*>(in2.buffer),
                             in2.shape(),
                             activation,
                             reinterpret_cast<float*>(out.buffer),
                             outShape);
    } else if (in1.type == OperandType::TENSOR_QUANT8_ASYMM) {
        success = addMulPrepare(in1.shape(), in2.shape(), &outShape) &&
                  setInfoAndAllocateIfNeeded(&out, outShape) &&
                  addQuant8(reinterpret_cast<const uint8_t*>(in1.buffer),
                            in1.shape(),
                            reinterpret_cast<const uint8_t*>(in2.buffer),
                            in2.shape(),
                            activation,
                            reinterpret_cast<uint8_t*>(out.buffer),
                            outShape);
    }

    if (!success) {
        ALOGE("OEM_OPERATION failed.");
        return ANEURALNETWORKS_OP_FAILED;
    }

    freeNoLongerUsedOperands(ins);
    return ANEURALNETWORKS_NO_ERROR;
}

} // namespace nn
} // namespace android
