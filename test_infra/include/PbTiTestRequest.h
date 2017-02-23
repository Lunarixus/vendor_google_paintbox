#ifndef PAINTBOX_TEST_INFRA_INCLUDE_PBTITESTREQUEST_H
#define PAINTBOX_TEST_INFRA_INCLUDE_PBTITESTREQUEST_H

#include <string>

namespace pbti {

/*
 * PbTiTestRequest defines test request from AP to Easel.
 */
struct PbTiTestRequest {
    // timeout seconds to run command.
    uint timeout_seconds;
    // absolute path for log file.
    std::string log_path;
    // command to run on Easel.
    std::string command;
};

}  // namespace pbti

#endif  // PAINTBOX_TEST_INFRA_INCLUDE_PBTITESTREQUEST_H
