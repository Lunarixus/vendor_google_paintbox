#ifndef PAINTBOX_TEST_INFRA_INCLUDE_PBTITESTREQUEST_H
#define PAINTBOX_TEST_INFRA_INCLUDE_PBTITESTREQUEST_H

#include <string>

namespace pbti {

/*
 * PbTiTestRequest defines test request from AP to Easel.
 */
struct PbTiTestRequest {
    // seconds set to run a test.
    int timeout_seconds;
    // absolute path for test log.
    std::string log_path;
    // command to run the test.
    std::string test_command;
};

}  // namespace pbti

#endif  // PAINTBOX_TEST_INFRA_INCLUDE_PBTITESTREQUEST_H
