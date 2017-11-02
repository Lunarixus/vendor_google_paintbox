#ifndef PAINTBOX_EASELCOMM2_BUFFER
#define PAINTBOX_EASELCOMM2_BUFFER

#include <string>

namespace EaselComm2 {

// Abstraction of device buffers supported in EaselComm2
// for buffer transfering on PCIe.
// Data structure is similar to hidl_memory.
// Buffer could be specified either by vaddr or ionFd.
// If both are valid, vaddr will override ionFd.
struct HardwareBuffer {
  void* vaddr;
  int ionFd;
  size_t size;
  int id;  // optional buffer id to note transferring sequence.

  HardwareBuffer();
  HardwareBuffer(void* vaddr, size_t size, int id = 0);
  HardwareBuffer(int ionFd, size_t size, int id = 0);

  // Returns true if HardwareBuffer is ion based, otherwise false.
  bool isIonBuffer();

  // Loads file into HardwareBuffer
  // Returns buffer vaddr if successful, otherwise nullptr.
  // User needs to free the returned buffer after use.
  void* loadFile(const std::string& filePath);

  // Saves buffer into a file.
  // Returns 0 if successful, otherwise error code.
  int saveFile(const std::string& filePath);
};

}  // namespace EaselComm2

#endif  // PAINTBOX_EASELCOMM2_BUFFER