#ifndef PAINTBOX_TEST_INFRA_PBTICLIENT_INCLUDE_PBTICLIENTRUNNER_H
#define PAINTBOX_TEST_INFRA_PBTICLIENT_INCLUDE_PBTICLIENTRUNNER_H

#include "PbTiClient.h"

class PbTiClientRunner : public android::PbTiClientListener {
 public:
    PbTiClientRunner();

    ~PbTiClientRunner();

    // Override PbTiClientListener::onPbTiTestResult to receive test results.
    void onPbTiTestResult(const std::string &result) override;

    // Connect to client.
    android::status_t connectClient();

    // Submit test requests.
    android::status_t submitPbTiTestRequest(const pbti::PbTiTestRequest &request);

 private:
    android::PbTiClient mClient;

    // Flag indicating if the test is connected to PbTiClient.
    bool mConnected;
};

#endif  // PAINTBOX_TEST_INFRA_PBTICLIENT_INCLUDE_PBTICLIENTRUNNER_H
