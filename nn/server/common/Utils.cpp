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

#define LOG_TAG "Utils"

#include "Utils.h"
#include "NeuralNetworks.h"

#include <android-base/logging.h>

namespace paintbox_nn {

const char* kOperationNames[ANEURALNETWORKS_NUMBER_OPERATION_TYPES] = {
        "AVERAGE_POOL",
        "CONCATENATION",
        "CONV",
        "DEPTHWISE_CONV",
        "MAX_POOL",
        "L2_POOL",
        "DEPTH_TO_SPACE",
        "SPACE_TO_DEPTH",
        "LOCAL_RESPONSE_NORMALIZATION",
        "SOFTMAX",
        "RESHAPE",
        "SPLIT",
        "FAKE_QUANT",
        "ADD",
        "FULLY_CONNECTED",
        "CAST",
        "MUL",
        "L2_NORMALIZATION",
        "LOGISTIC",
        "RELU",
        "RELU6",
        "RELU1",
        "TANH",
        "DEQUANTIZE",
        "FLOOR",
        "GATHER",
        "RESIZE_BILINEAR",
        "LSH_PROJECTION",
        "LSTM",
        "SVDF",
        "RNN",
        "N_GRAM",
        "LOOKUP",
};

const char* getOperationName(OperationType type) {
    uint32_t n = static_cast<uint32_t>(type);
    nnAssert(n < ANEURALNETWORKS_NUMBER_OPERATION_TYPES);
    return kOperationNames[n];
}

const uint32_t kSizeOfDataType[ANEURALNETWORKS_NUMBER_DATA_TYPES]{
        2, // ANEURALNETWORKS_FLOAT16
        4, // ANEURALNETWORKS_FLOAT32
        1, // ANEURALNETWORKS_INT8
        1, // ANEURALNETWORKS_UINT8
        2, // ANEURALNETWORKS_INT16
        2, // ANEURALNETWORKS_UINT16
        4, // ANEURALNETWORKS_INT32
        4, // ANEURALNETWORKS_UINT32
        2, // ANEURALNETWORKS_TENSOR_FLOAT16
        4, // ANEURALNETWORKS_TENSOR_FLOAT32
        1  // ANEURALNETWORKS_TENSOR_SIMMETRICAL_QUANT8
};

uint32_t sizeOfData(OperandType type, const std::vector<uint32_t>& dimensions) {
    int n = static_cast<int>(type);
    nnAssert(n < ANEURALNETWORKS_NUMBER_DATA_TYPES);

    uint32_t size = kSizeOfDataType[n];
    for (auto d : dimensions) {
        size *= d;
    }
    return size;
}

} // namespace paintbox_nn