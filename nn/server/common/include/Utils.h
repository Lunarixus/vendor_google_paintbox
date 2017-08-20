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

#ifndef PAINTBOX_NN_COMMON_UTILS_H
#define PAINTBOX_NN_COMMON_UTILS_H

#include "vendor/google_paintbox/nn/shared/proto/types.pb.h"

#include <android-base/logging.h>
#include <vector>

namespace paintbox_nn {

// TODO Remove all the LOG(DEBUG) statements in all the files.

// Assert macro, as Android does not generally support assert.
#define nnAssert(v)                                                                            \
    do {                                                                                       \
        if (!(v)) {                                                                            \
            LOG(ERROR) << "nnAssert failed at " << __FILE__ << ":" << __LINE__ << " - '" << #v \
                       << "'\n";                                                               \
            abort();                                                                           \
        }                                                                                      \
    } while (0)

// Returns the the amount of space needed to store a tensor of the specified
// dimensions and type.
uint32_t sizeOfData(OperandType type, const std::vector<uint32_t>& dimensions);

// Returns the name of the operation in ASCII.
const char* getOperationName(OperationType opCode);

inline std::string toString(uint32_t obj) {
    return std::to_string(obj);
}

template <typename Type>
std::string toString(const std::vector<Type>& range) {
    std::string os = "[";
    for (size_t i = 0; i < range.size(); ++i) {
        os += (i == 0 ? "" : ", ") + toString(range[i]);
    }
    return os += "]";
}

} // namespace paintbox_nn

#endif // PAINTBOX_NN_COMMON_UTILS_H