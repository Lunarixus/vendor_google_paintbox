#ifndef PAINTBOX_EASEL_SERVICE_ID_H
#define PAINTBOX_EASEL_SERVICE_ID_H

#include <uapi/linux/google-easel-comm.h>

/*
 * Easel service identifiers registered by clients and servers to
 * route messages to each other.
 */
enum EaselService {
  // Easel system control
  EASEL_SERVICE_SYSCTRL = 0,
  // Easel shell
  EASEL_SERVICE_SHELL,
  // Used by unit tests
  EASEL_SERVICE_TEST,
  // HDR+ via Paintbox camera framework service
  EASEL_SERVICE_HDRPLUS,
  // Logging service.
  EASEL_SERVICE_LOG,
  // Max service ID
  EASEL_SERVICE_MAX = EASELCOMM_SERVICE_COUNT - 1,
};

#endif  // PAINTBOX_EASEL_SERVICE_ID_H