#define LOG_TAG "EaselComm2::Buffer"

#include "EaselComm2Buffer.h"

#include "log/log.h"

#include <fstream>

namespace EaselComm2 {

HardwareBuffer::HardwareBuffer() { reset(); }

HardwareBuffer::HardwareBuffer(void* vaddr, size_t size, int id)
    : mVaddr(vaddr), mIonFd(-1), mSize(size), mId(id), mAllocBuffer(false) {}

HardwareBuffer::HardwareBuffer(int ionFd, size_t size, int id)
    : mVaddr(nullptr),
      mIonFd(ionFd),
      mSize(size),
      mId(id),
      mAllocBuffer(false) {}

HardwareBuffer::HardwareBuffer(size_t size, int id) {
  reset();
  allocBuffer(size);
  mId = id;
}

HardwareBuffer::HardwareBuffer(const std::string& filePath, int id) {
  reset();
  loadFile(filePath);
  mId = id;
}

HardwareBuffer::~HardwareBuffer() {
  if (mAllocBuffer && (mVaddr != nullptr)) {
    free(mVaddr);
  }
}

bool HardwareBuffer::isIonBuffer() const {
  return mVaddr == nullptr && mIonFd > 0;
}

int HardwareBuffer::saveFile(const std::string& filePath) {
  if (isIonBuffer()) return -EINVAL;
  std::ofstream output(filePath, std::ofstream::binary);
  if (!output.is_open()) return -ENOENT;
  output.write(static_cast<char*>(mVaddr), mSize);
  output.close();
  return 0;
}

bool HardwareBuffer::valid() const {
  return mSize > 0 && (mIonFd >= 0 || mVaddr != nullptr);
}

void HardwareBuffer::reset() {
  mVaddr = nullptr;
  mIonFd = -1;
  mSize = 0;
  mId = 0;
  mAllocBuffer = false;
}

bool HardwareBuffer::allocBuffer(size_t size) {
  if (mVaddr != nullptr || mIonFd >= 0 || mSize > 0) {
    ALOGE("buffer not empty");
    return false;
  }
  void* buffer = malloc(size);
  if (buffer == nullptr) {
    ALOGE("could not allocate buffer");
    return false;
  }
  mVaddr = buffer;
  mIonFd = -1;
  mSize = size;
  mAllocBuffer = true;
  return true;
}

int HardwareBuffer::loadFile(const std::string& filePath) {
  if (mVaddr != nullptr || mSize != 0) {
    ALOGE("%s: buffer not empty!", __FUNCTION__);
    return -EINVAL;
  }

  std::ifstream input(filePath, std::ifstream::binary);
  if (!input.is_open()) {
    ALOGE("%s: cannot open file %s!", __FUNCTION__, filePath.c_str());
    return -ENOENT;
  }

  input.seekg(0, input.end);
  size_t fileSize = input.tellg();
  input.seekg(0);

  bool success = allocBuffer(fileSize);
  if (!success) return -ENOMEM;
  input.read(static_cast<char*>(mVaddr), fileSize);
  input.close();
  return 0;
}

void* HardwareBuffer::vaddr() { return mVaddr; }
const void* HardwareBuffer::vaddr() const { return mVaddr; }
int HardwareBuffer::ionFd() const { return mIonFd; }
size_t HardwareBuffer::size() const { return mSize; }
int HardwareBuffer::id() const { return mId; }

void HardwareBuffer::setId(int id) { mId = id; };
}  // namespace EaselComm2
