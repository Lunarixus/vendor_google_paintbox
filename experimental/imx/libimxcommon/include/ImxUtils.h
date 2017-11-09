/*
 * Copyright 2017 The Android Open Source Project
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

#ifndef IMX_UTILS_H
#define IMX_UTILS_H

#include <stdint.h>

namespace imx {

class BufferRecord {
public:
    BufferRecord(size_t size_bytes);
    BufferRecord();
    BufferRecord(BufferRecord&& that);
    ~BufferRecord();

    BufferRecord& operator=(BufferRecord&& that);

    size_t size_bytes() const { return size_bytes_; }
    void* vaddr() { return vaddr_; }
    const void* vaddr() const { return vaddr_; }

private:
    void free();
    BufferRecord(const BufferRecord& that) = delete;
    const BufferRecord& operator=(const BufferRecord& that) = delete;
    size_t size_bytes_;
    void* vaddr_;
};

}  // namespace imx

#endif
