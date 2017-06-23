#ifndef PAINTBOX_TEST_INFRA_PBTICLIENT_INCLUDE_PBTICLIENTRUNNER_H
#define PAINTBOX_TEST_INFRA_PBTICLIENT_INCLUDE_PBTICLIENTRUNNER_H

#include "PbTiClient.h"

class PbTiClientRunner : public android::PbTiClientListener {
 public:
    PbTiClientRunner();

    ~PbTiClientRunner();

    // Override PbTiClientListener::onPbTiTestResult to receive test results.
    void onPbTiTestResult(const std::string &result) override;

    void onPbTiTestResultFailed() override;

    /*
     * Activate Easel.
     *
     * Returns:
     *  0:          on success.
     *  -NO_INIT:   if it's not activated.
     */
    android::status_t activate();

    /*
     * Deactivate Easel.
     *
     * Returns:
     *  0:          on success.
     *  -NO_INIT:   if it's not deactivated.
     */
    android::status_t deactivate();

    // Connect to client.
    android::status_t connectClient();

    // Submit test requests.
    android::status_t submitPbTiTestRequest(const pbti::PbTiTestRequest &request);

    /*
     * Wait for the service to finish.
     * PbTi client should not return until test log file is received.
     */
    void wait();

 private:
    android::PbTiClient mClient;

    // Indicate if Easel is activated.
    bool mEaselActivated;
    // Flag indicating if the test is connected to PbTiClient.
    bool mConnected;
    std::mutex mExitLock;
    std::condition_variable mExitCondition;
};

#endif  // PAINTBOX_TEST_INFRA_PBTICLIENT_INCLUDE_PBTICLIENTRUNNER_H
