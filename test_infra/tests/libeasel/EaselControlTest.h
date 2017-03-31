#ifndef ANDROID_EASEL_CONTROL_TEST_H
#define ANDROID_EASEL_CONTROL_TEST_H

const int kEmptyHandlerId = 0;
const int kEchoHandlerId = 1;
const int kCaseHandlerId = 2;
const int kEndHandlerId = 3;

enum TestType {
  EMPTY,
  ECHO,
  UPPER,
  LOWER,
  END,
};

#define TEXT_LENGTH 1024

struct EmptyRequest {};

struct EmptyResponse {};

struct TextRequest {
  int id;
  char text[TEXT_LENGTH];
};

struct TextResponse {
  int id;
  char text[TEXT_LENGTH];
};


struct EndRequest {};

#endif  // ANDROID_EASEL_CONTROL_TEST_H
