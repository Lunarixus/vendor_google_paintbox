#ifndef PAINTBOX_EASELCOMM2_TEST_H
#define PAINTBOX_EASELCOMM2_TEST_H

const int kProtoChannel = 0;
const int kStringChannel = 1;
const int kStructChannel = 2;
const int kIonBufferChannel = 3;
const int kMallocBufferChannel = 4;

struct TestStruct {
  int number;
  bool flag;
};

#endif  // PAINTBOX_EASELCOMM2_TEST_H