#ifndef PAINTBOX_TEST_INFRA_LIBPBTICLIENT_INCLUDE_PBTICLIENTLISTENER_H
#define PAINTBOX_TEST_INFRA_LIBPBTICLIENT_INCLUDE_PBTICLIENTLISTENER_H

namespace android {

/*
 * PbTiClientListener defines callbacks that will be invoked by PbTiClient for
 * events like returning test results.
 */
class PbTiClientListener {
 public:
    virtual ~PbTiClientListener() {}

    /*
     * Invoked when a PbTi test result for a PbTi test request is received.
     */
    virtual void onPbTiTestResult(const std::string &result) = 0;

    /*
     * Invoked when a PbTi test request is failed.
     */
    virtual void onPbTiTestResultFailed() = 0;
};

}  // namespace android

#endif  // PAINTBOX_TEST_INFRA_LIBPBTICLIENT_INCLUDE_PBTICLIENTLISTENER_H
