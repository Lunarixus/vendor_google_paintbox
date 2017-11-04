#ifndef PAINTBOX_EASELCOMM2_BUFFER
#define PAINTBOX_EASELCOMM2_BUFFER

#include <string>

namespace EaselComm2 {

// Abstraction of device buffers supported in EaselComm2
// for buffer transfering on PCIe.
// Data structure is similar to hidl_memory.
// Buffer could be specified either by vaddr or ionFd.
// If both are valid, vaddr will override ionFd.
class HardwareBuffer {
 public:
  // Creates an empty, invalid buffer.
  HardwareBuffer();
  // Wraps a malloc hardware buffer.
  HardwareBuffer(void* vaddr, size_t size, int id = 0);
  // Wraps a ion hardware buffer.
  HardwareBuffer(int ionFd, size_t size, int id = 0);
  // Creates a malloc hardware buffer with specified buffer size.
  // This malloc buffer is owned by the HardwareBuffer object and will be freed
  // in destructor. User don't manage the internal malloced buffer.
  HardwareBuffer(size_t size, int id = 0);
  // Creates a malloc hardware buffer loading the file from filePath.
  // This malloc buffer is owned by the HardwareBuffer object and will be freed
  // in destructor. User don't manage the internal malloced buffer.
  HardwareBuffer(const std::string& filePath, int id = 0);
  // Frees the malloc buffer if allocated and owned.
  ~HardwareBuffer();

  // Returns true if HardwareBuffer is ion based, otherwise false.
  bool isIonBuffer() const;

  // Returns true if buffer is valid and not empty, otherwise false.
  bool valid() const;

  // Saves buffer into a file.
  // Returns 0 if successful, otherwise error code.
  int saveFile(const std::string& filePath);

  // Returns the vaddr of the buffer, nullptr if it is not malloc buffer.
  void* vaddr();
  const void* vaddr() const;

  // Returns the ion fd of the buffer or -1 if it is not ion buffer.
  int ionFd() const;

  // Returns the size of the buffer.
  size_t size() const;

  // Returns the id of the buffer.
  int id() const;

  // Sets the id of the buffer.
  void setId(int id);

 private:
  // Clears the buffer member attributes.
  void reset();

  // Allocates a malloc buffer with size and assigns to this hardware buffer.
  // Returns true if successful, other wise false.
  bool allocBuffer(size_t size);

  // Loads file into HardwareBuffer.
  // Returns 0 if successful, otherwise error code.
  int loadFile(const std::string& filePath);

  void* mVaddr;
  int mIonFd;
  size_t mSize;
  int mId;  // optional buffer id to note transferring sequence.

  // Flags whether vaddr is owned by this hardware buffer or not.
  bool mAllocBuffer;
};

}  // namespace EaselComm2
#endif  // PAINTBOX_EASELCOMM2_BUFFER
