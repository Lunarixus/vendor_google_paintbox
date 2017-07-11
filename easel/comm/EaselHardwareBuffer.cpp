#include "EaselHardwareBuffer.h"

namespace EaselComm2 {
HardwareBuffer::HardwareBuffer(int ionFd, const Desc& desc)
    : mIonFd(ionFd), mDesc(desc) {}

HardwareBuffer::Desc HardwareBuffer::desc() const { return mDesc; }

int HardwareBuffer::ionFd() const { return mIonFd; }

size_t HardwareBuffer::size() const { return size(mDesc); }

size_t HardwareBuffer::size(const HardwareBuffer::Desc& desc) {
  return desc.stride * desc.height * desc.layers * desc.bits_per_pixel / 8;
}

bool HardwareBuffer::Desc::operator==(const Desc& other) {
  return (width == other.width && height == other.height &&
    stride == other.stride && layers == other.layers &&
    bits_per_pixel == other.bits_per_pixel);
}

bool HardwareBuffer::Desc::operator!=(const Desc& other) {
  return !(*this == other);
}
}  // namespace EaselComm2
