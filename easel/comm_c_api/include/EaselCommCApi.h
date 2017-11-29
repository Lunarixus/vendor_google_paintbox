#ifndef HARDWARE_GCHIPS_PAINTBOX_IMX_TESTS_FAKES_EASEL_COMM_C_API_H_
#define HARDWARE_GCHIPS_PAINTBOX_IMX_TESTS_FAKES_EASEL_COMM_C_API_H_

// This file should not be edited in Android and should be identical to
// easel_comm_c_api.h in Google3.

#include <stddef.h>

// A C API interface to easel_comm. This CL enables us to build in Google3
// and run in Android while waiting for easel_comm to migrate to Google3.
// Eventually, we will build entirely in Google3, and this goes away.
#ifdef __cplusplus
extern "C" {
#endif

#define EccInvalidHandle (void*)0

typedef void* EccHandle;
typedef void* EccMessageHandle;

typedef int EccServiceId;

typedef void (*EccHandler)(int channel_id, EccMessageHandle message,
                           void* user_data);

typedef enum { CLIENT_ECC_MODE, SERVER_ECC_MODE } EccMode;

typedef struct {
  void* vaddr;
  int ion_fd;
  size_t size;
} EccHardwareBuffer;

// HardwareBuffer abstractions
EccHardwareBuffer EccCreateHardwareBufferWithFd(int ion_fd, size_t size);

EccHardwareBuffer EccCreateHardwareBufferWithVaddr(void* vaddr, size_t size);

// Message abstractions
const void* EccGetMessageBody(EccMessageHandle message_handle);

size_t EccGetMessageBodySize(EccMessageHandle message_handle);

// EaselComm abstractions
void EccCreate(EccMode mode, EccHandle* ecc_client_handle_ptr);

int EccOpen(EccHandle ecc_handle, EccServiceId service_id);

int EccOpenPersistent(EccHandle ecc_handle, EccServiceId service_id);

void EccClose(EccHandle ecc_handle);

int EccStartReceiving(EccHandle ecc_handle);

void EccJoinReceiving(EccHandle ecc_handle);

int EccSendWithPayload(EccHandle ecc_handle, int channel_id, const void* body,
                       size_t body_size, EccHardwareBuffer payload);

int EccSend(EccHandle ecc_handle, int channel_id, const void* body,
            size_t body_size);

void EccRegisterHandler(EccHandle ecc_handle, int channel_id,
                        EccHandler ecc_handler, void* user_data);

int EccReceivePayload(EccHandle ecc_handle, EccMessageHandle message,
                      EccHardwareBuffer buffer);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  // HARDWARE_GCHIPS_PAINTBOX_IMX_TESTS_FAKES_EASEL_COMM_C_API_H_

