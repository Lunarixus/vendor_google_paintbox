#define LOG_TAG "EaselControlTest"

#include <gtest/gtest.h>
#include <log/log.h>
#include <utils/Errors.h>

#include "easelcontrol.h"
#include "EaselControlTest.h"


namespace android {

class EaselControlTest : public ::testing::Test {
 public:
  void echo(const std::string &text) {
    sendRequestWithCallback(kEchoHandlerId,
        ECHO,
        text,
        [text, this](const ControlData &response) {
      ASSERT_EQ(sizeof(TextResponse), response.size);
      auto *textResponse = response.getImmutable<TextResponse>();
      EXPECT_EQ(text, std::string(textResponse->text));
      std::unique_lock<std::mutex> lock(m);
      received++;
      received_cond.notify_one();
    });
  }

  void changeCase(const std::string &text, bool upper) {
    sendRequestWithCallback(kCaseHandlerId,
        upper ? UPPER : LOWER,
        text,
        [text, upper, this](const ControlData &response) {
      std::string new_text = text;
      if (upper) {
        transform(new_text.begin(), new_text.end(), new_text.begin(), ::toupper);
      } else {
        transform(new_text.begin(), new_text.end(), new_text.begin(), ::tolower);
      }
      ASSERT_EQ(sizeof(TextResponse), response.size);
      auto *textResponse = response.getImmutable<TextResponse>();
      EXPECT_EQ(new_text, std::string(textResponse->text));
      std::unique_lock<std::mutex> lock(m);
      received++;
      received_cond.notify_one();
    });
  }

  void empty() {
    sendEmptyRequestWithCallback(kEmptyHandlerId,
        [this](const ControlData &response) {
      EXPECT_NE(nullptr, response.getImmutable<EmptyResponse>());
      std::unique_lock<std::mutex> lock(m);
      received++;
      received_cond.notify_one();
    });
  }

  void end() {
    sendEndRequest(kEndHandlerId);
  }

  void waitForCallbackSuccess(int target_received, int timeoutMs) {
    std::unique_lock<std::mutex> lock(m);
    while (received < target_received) {
      EXPECT_EQ(std::cv_status::no_timeout, received_cond.wait_for(
          lock, std::chrono::milliseconds(timeoutMs)));
    }
  }

  void waitForCallbackFail(int target_received, int timeoutMs) {
    std::unique_lock<std::mutex> lock(m);
    while (received < target_received) {
      EXPECT_EQ(std::cv_status::timeout, received_cond.wait_for(
          lock, std::chrono::milliseconds(timeoutMs)));
      break;
    }
  }

 protected:
  void SetUp() override {
    int ret = client.open();
    ASSERT_EQ(OK, ret);
    ret = client.resume();
    ASSERT_EQ(OK, ret);
    ret = client.activate();
    ASSERT_EQ(OK, ret);

    received = 0;
  }

  void TearDown() override {
    int ret = client.deactivate();
    ASSERT_EQ(OK, ret);
    ret = client.suspend();
    ASSERT_EQ(OK, ret);
    client.close();
  }

 private:
  EaselControlClient client;

  int received;
  std::mutex m;
  std::condition_variable received_cond;

  void sendEndRequest(int handlerId) {
    EndRequest endRequest;
    ControlData request(endRequest);
    int ret = client.sendRequest(handlerId, END, request);
    EXPECT_EQ(0, ret);
  }

  void sendEmptyRequestWithCallback(int handlerId,
      std::function<void(const ControlData &response)> callback) {
    EmptyRequest emptyRequest;
    ControlData request(emptyRequest);
    int ret = client.sendRequestWithCallback(handlerId, EMPTY, request, callback);
    EXPECT_EQ(0, ret);
  }

  void sendRequest(int handlerId, TestType type, const std::string &s) {
    static int id = 0;
    TextRequest textRequest;
    textRequest.id = id++;
    size_t size = s.copy(textRequest.text, 1024);
    textRequest.text[size] = '\0';
    ControlData request(textRequest);
    int ret = client.sendRequest(handlerId, type, request);
    EXPECT_EQ(0, ret);
  }

  void sendRequestWithCallback(int handlerId, TestType type, const std::string &s,
      std::function<void(const ControlData &response)> callback) {

    static int id = 0;
    TextRequest textRequest;
    textRequest.id = id++;
    size_t size = s.copy(textRequest.text, 1024);
    textRequest.text[size] = '\0';
    ControlData request(textRequest);
    int ret = client.sendRequestWithCallback(handlerId, type, request, callback);
    EXPECT_EQ(0, ret);
  }
};


TEST_F(EaselControlTest, Empty) {
  empty();
  waitForCallbackSuccess(1, 10);
}

TEST_F(EaselControlTest, EchoSingle) {
  echo("echo test");
  waitForCallbackSuccess(1, 10);
}

TEST_F(EaselControlTest, EchoMultiple) {
  const int kEchoSize = 100;
  for (int i = 0; i < kEchoSize; i++) {
    std::string text = "echo test" + std::to_string(i);
    echo(text);
  }
  waitForCallbackSuccess(kEchoSize, 10);
}

TEST_F(EaselControlTest, MultipleTypes) {
  changeCase("TeXt", true);
  changeCase("TeXt", false);
  waitForCallbackSuccess(2, 10);
}

TEST_F(EaselControlTest, End) {
  end();
  echo("echo text");
  waitForCallbackFail(1, 10);
}

}  // namespace android
