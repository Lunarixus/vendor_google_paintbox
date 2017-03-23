#include <gtest/gtest.h>
#include <log/log.h>
#include <time.h>

const static int64_t kSecToNs = 1e9;

class LogTest : public ::testing::Test {

public:
  long getLogDelayNs(int iterations, long sleep_ns, const std::string& log) {
    struct timespec start, end, sleep;
    sleep.tv_sec = sleep_ns / kSecToNs;
    sleep.tv_nsec = sleep_ns - sleep.tv_sec * kSecToNs;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < iterations; i++) {
      __android_log_print(ANDROID_LOG_ERROR, "LogTest", "message %d: %s", i, log.c_str());
      nanosleep(&sleep, nullptr);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    long delay_ns = end.tv_sec * kSecToNs + end.tv_nsec - start.tv_sec * kSecToNs - start.tv_nsec;
    return delay_ns / iterations - sleep_ns;
  }

  std::string getLogString(int size) {
    std::string log;
    char c = 'a';
    for (int i = 0; i < size; i++) {
      log += c;
      c++;
      if (c == 'z' + 1) {
        c = 'a';
      }
    }
    return log;
  }
};

TEST_F(LogTest, Performance) {
  const static int kLogSize = 4096;
  long delay_ns = getLogDelayNs(1e3, 1e6, getLogString(kLogSize));
  __android_log_print(ANDROID_LOG_INFO, "LogTest",
      "Delay %lu ns for log buffer with size %d", delay_ns, kLogSize);
  EXPECT_LT(delay_ns, 1e6);
}