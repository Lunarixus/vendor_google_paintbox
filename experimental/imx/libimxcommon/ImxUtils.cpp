#define LOG_TAG "ImxUtils"
#include <log/log.h>

#include "ImxUtils.h"

#include <utility>

namespace imx {

BufferRecord::BufferRecord(size_t size_bytes)
    : size_bytes_(size_bytes) {
    // In the event of unsuccessful allocation, reset size bytes.
    vaddr_ = new unsigned char[size_bytes];
    if (!vaddr_) {
        ALOGE("%s: failed to allocate cpu backing store", __FUNCTION__);
        size_bytes_ = 0;
    }
}

BufferRecord::BufferRecord() : size_bytes_(0), vaddr_(nullptr) {}

BufferRecord::BufferRecord(BufferRecord&& that) {
    *this = std::move(that);
}

BufferRecord::~BufferRecord() {
    free();
}

BufferRecord& BufferRecord::operator=(BufferRecord&& that) {
    free();
    vaddr_ = that.vaddr_;
    size_bytes_ = that.size_bytes_;

    that.vaddr_ = nullptr;
    that.size_bytes_ = 0;
    return *this;
}

void BufferRecord::free() {
    if (vaddr_) {
        delete[] (unsigned char*)vaddr_;
        vaddr_ = nullptr;
        size_bytes_ = 0;
    }
}

}  // namespace imx
