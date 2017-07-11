#ifndef PAINTBOX_EASEL_HARDWARE_BUFFER_H
#define PAINTBOX_EASEL_HARDWARE_BUFFER_H

#include <cstddef>
#include <cstdint>

namespace EaselComm2 {

// Abstraction of device buffers supported in EaselComm2
// for buffer transfering on PCIe.
class HardwareBuffer {
 public:
  struct Desc {
    uint32_t width;   // width in pixels.
    uint32_t stride;  // stride in pixels.
    uint32_t height;
    uint32_t layers;
    uint32_t bits_per_pixel;

    bool operator==(const Desc& other);
    bool operator!=(const Desc& other);
  };

  HardwareBuffer(int ionFd, const Desc& desc);

  // Returns the buffer description
  Desc desc() const;

  // Returns the ion fd of the buffer;
  int ionFd() const;

  // Returns the size of the buffer in bytes;
  size_t size() const;

  // Returns the size in bytes needed to hold a buffer with this desc.
  static size_t size(const Desc& desc);

 private:
  const int mIonFd;
  const Desc mDesc;
};
}  // namespace EaselComm2

#endif  // PAINTBOX_EASEL_HARDWARE_BUFFER_H