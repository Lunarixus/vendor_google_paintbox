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

namespace android {
namespace nn {

namespace {

template <typename EntryType, uint32_t entryCount, uint32_t entryCountOEM>
EntryType tableLookup(const EntryType (&table)[entryCount],
                      const EntryType (&tableOEM)[entryCountOEM],
                      uint32_t code) {
    if (code < entryCount) {
        return table[code];
    } else if (code >= kOEMCodeBase && (code - kOEMCodeBase) < entryCountOEM) {
        return tableOEM[code - kOEMCodeBase];
    } else {
        nnAssert(!"tableLookup: bad code");
        return EntryType();
    }
}

};  // anonymous namespace

const uint32_t kSizeOfDataType[]{
        4, // ANEURALNETWORKS_FLOAT32
        4, // ANEURALNETWORKS_INT32
        4, // ANEURALNETWORKS_UINT32
        4, // ANEURALNETWORKS_TENSOR_FLOAT32
        4, // ANEURALNETWORKS_TENSOR_INT32
        1  // ANEURALNETWORKS_TENSOR_SYMMETRICAL_QUANT8
};

const bool kScalarDataType[]{
        true,  // ANEURALNETWORKS_FLOAT32
        true,  // ANEURALNETWORKS_INT32
        true,  // ANEURALNETWORKS_UINT32
        false, // ANEURALNETWORKS_TENSOR_FLOAT32
        false, // ANEURALNETWORKS_TENSOR_INT32
        false, // ANEURALNETWORKS_TENSOR_SYMMETRICAL_QUANT8
};

const uint32_t kSizeOfDataTypeOEM[]{
        0, // ANEURALNETWORKS_OEM
        1, // ANEURALNETWORKS_TENSOR_OEM_BYTE
};

const bool kScalarDataTypeOEM[]{
        true,  // ANEURALNETWORKS_OEM
        false, // ANEURALNETWORKS_TENSOR_OEM_BYTE
};


uint32_t sizeOfData(OperandType type, const std::vector<uint32_t>& dimensions) {
    int n = static_cast<int>(type);

    uint32_t size = tableLookup(kSizeOfDataType, kSizeOfDataTypeOEM, n);

    if (tableLookup(kScalarDataType, kScalarDataTypeOEM, n) == true) {
        return size;
    }

    for (auto d : dimensions) {
        size *= d;
    }
    return size;
}

} // namespace nn
} // namespace android
