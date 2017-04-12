#define LOG_TAG "EaselControlTestServer"

#include <assert.h>
#include <condition_variable>
#include <ctype.h>
#include <string>

#include "easelcontrol.h"
#include "EaselControlTest.h"
#include "utils/Log.h"

// Test server for EaselControl RpcMsg interface.
// Test server will stop when received END message

class EmptyHandler : public RequestHandler {
 public:
  void handleRequest (
      int rpcId,
      const ControlData &,
      ControlData *response) override {
    switch (rpcId) {
      case EMPTY:
        if (response != nullptr) {
          response->getMutable<EmptyResponse>();
        }
        ALOGI("server: empty message received");
        break;

      default:
        break;
    }
  }
};

class EchoHandler : public RequestHandler {
 public:
  void handleRequest (
      int rpcId,
      const ControlData &request,
      ControlData *response) override {
    switch (rpcId) {
      case ECHO: {
        auto *textRequest = request.getImmutable<TextRequest>();
        if (response != nullptr) {
          auto *textResponse = response->getMutable<TextResponse>();
          memcpy(textResponse, textRequest, sizeof(TextRequest));
        }

        ALOGI("%d: %s", textRequest->id, textRequest->text);
        break;
      }

      default:
        break;
    }
  }
};

class CaseHandler : public RequestHandler {
 public:
  void handleRequest (
      int rpcId,
      const ControlData &request,
      ControlData *response) override {
    ALOG_ASSERT(request.size == sizeof(TextRequest));
    auto *textRequest = request.getImmutable<TextRequest>();
    std::string text(textRequest->text);

    switch (rpcId) {
      case UPPER:
        changeCase(text, true);
        break;
      case LOWER:
        changeCase(text, false);
        break;
      default:
        break;
    }

    if (response != nullptr) {
      auto *textResponse = response->getMutable<TextResponse>();
      auto size = text.copy(textResponse->text, text.size());
      textResponse->text[size] = '\0';
    }

    ALOGI("%d: %s", textRequest->id, text.data());
  }

 private:
  void changeCase(std::string &s, bool upperCase) {
    for (size_t i = 0; i < s.size(); i++) {
      s[i] = upperCase ? toupper(s[i]) : tolower(s[i]);
    }
  }
};

class EndHandler : public RequestHandler {
 public:
  EndHandler(std::condition_variable *received) : mReceived(received) {};

  void handleRequest (
      int rpcId,
      const ControlData &,
      ControlData *) override {
    switch (rpcId) {
      case END:
        mReceived->notify_one();
        break;
      default:
        break;
    }
  }

 private:
  std::condition_variable *mReceived;
};

int main() {
  EaselControlServer server;
  int ret = server.open();
  if (ret != 0) {
    fprintf(stderr, "Could not open server %d\n", ret);
    exit(ret);
  }

  EmptyHandler emptyHandler;
  EchoHandler echoHandler;
  CaseHandler caseHandler;
  std::condition_variable received;
  EndHandler endHandler(&received);
  server.registerHandler(&emptyHandler, kEmptyHandlerId);
  server.registerHandler(&echoHandler, kEchoHandlerId);
  server.registerHandler(&caseHandler, kCaseHandlerId);
  server.registerHandler(&endHandler, kEndHandlerId);

  std::mutex m;
  std::unique_lock<std::mutex> lock(m);
  received.wait(lock);
  server.close();
}
