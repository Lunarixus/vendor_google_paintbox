#ifndef HARDWARE_GCHIPS_PAINTBOX_SYSTEM_INCLUDE_EASEL_COMM_H_
#define HARDWARE_GCHIPS_PAINTBOX_SYSTEM_INCLUDE_EASEL_COMM_H_

// Easel service identifiers registered by clients and servers to
// route messages to each other.
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
  // NN service
  EASEL_SERVICE_NN,
  // EaselManager service
  EASEL_SERVICE_MANAGER,
};

#endif  // HARDWARE_GCHIPS_PAINTBOX_SYSTEM_INCLUDE_EASEL_COMM_H_
