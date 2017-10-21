#ifndef PAINTBOX_EASELCOMM2_TEST_H
#define PAINTBOX_EASELCOMM2_TEST_H

#include <cstddef>
#include <cstdint>

const int kProtoChannel = 0;
const int kStringChannel = 1;
const int kStructChannel = 2;
const int kIonBufferChannel = 3;
const int kMallocBufferChannel = 4;
const int kFileChannel = 5;
const int kPingChannel = 6;

struct TestStruct {
  int number;
  bool flag;
};

struct FileStruct {
  uint64_t size;
};

#endif  // PAINTBOX_EASELCOMM2_TEST_H