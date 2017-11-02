#include "EaselComm2Buffer.h"

#include "log/log.h"

#include <fstream>

namespace EaselComm2 {

HardwareBuffer::HardwareBuffer() : vaddr(nullptr), ionFd(-1), size(0), id(0) {}
HardwareBuffer::HardwareBuffer(void* vaddr, size_t size, int id)
    : vaddr(vaddr), ionFd(-1), size(size), id(id) {}
HardwareBuffer::HardwareBuffer(int ionFd, size_t size, int id)
    : vaddr(nullptr), ionFd(ionFd), size(size), id(id) {}

bool HardwareBuffer::isIonBuffer() { return vaddr == nullptr && ionFd > 0; }

void* HardwareBuffer::loadFile(const std::string& filePath) {
  if (vaddr != nullptr || size != 0) {
    ALOGE("%s: buffer not empty!", __FUNCTION__);
    return nullptr;
  }

  std::ifstream input(filePath, std::ifstream::binary);
  if (!input.is_open()) {
    ALOGE("%s: cannot open file %s!", __FUNCTION__, filePath.c_str());
    return nullptr;
  }

  input.seekg(0, input.end);
  size_t fileSize = input.tellg();
  input.seekg(0);
  void* buffer = malloc(fileSize);
  if (buffer == nullptr) {
    ALOGE("%s: cannot allocate buffer", __FUNCTION__);
    return nullptr;
  }

  input.read(static_cast<char*>(buffer), fileSize);

  vaddr = buffer;
  size = fileSize;
  ionFd = -1;
  input.close();
  return buffer;
}

int HardwareBuffer::saveFile(const std::string& filePath) {
  if (isIonBuffer()) return -EINVAL;
  std::ofstream output(filePath, std::ofstream::binary);
  if (!output.is_open()) return -ENOENT;
  output.write(static_cast<char*>(vaddr), size);
  output.close();
  return 0;
}
}  // namespace EaselComm2
