#include "EaselCommCApi.h"

#include <cassert>

#include "EaselComm2.h"

static EaselComm2::Comm::Mode ecc_to_EaselComm2_mode(EccMode mode) {
  switch (mode) {
    case CLIENT_ECC_MODE:
      return EaselComm2::Comm::Mode::CLIENT;
    case SERVER_ECC_MODE:
      return EaselComm2::Comm::Mode::SERVER;
  }
}

static EaselService ecc_service_to_easel_service(EccServiceId ecc_service) {
  return static_cast<EaselService>(ecc_service);
}

static EaselComm2::Comm* get_client(EccHandle ecc_handle) {
  return reinterpret_cast<EaselComm2::Comm*>(ecc_handle);
}

static EaselComm2::Message* get_message(EccMessageHandle ecc_message_handle) {
  return reinterpret_cast<EaselComm2::Message*>(ecc_message_handle);
}

static EaselComm2::HardwareBuffer get_hardware_buffer(
    EccHardwareBuffer hardware_buffer) {
  if (hardware_buffer.vaddr) {
    return EaselComm2::HardwareBuffer(hardware_buffer.vaddr,
                                      hardware_buffer.size);
  } else {
    return EaselComm2::HardwareBuffer(hardware_buffer.ion_fd,
                                      hardware_buffer.size);
  }
}

static EccMessageHandle get_message_handle(const EaselComm2::Message& message) {
  // const_cast is safe is this is just an opaque handle.
  return reinterpret_cast<EccMessageHandle>(
      const_cast<EaselComm2::Message*>(&message));
}

EccHardwareBuffer EccCreateHardwareBufferWithFd(int ion_fd, size_t size) {
  EccHardwareBuffer buf;
  buf.vaddr = nullptr;
  buf.ion_fd = ion_fd;
  buf.size = size;
  return buf;
}

EccHardwareBuffer EccCreateHardwareBufferWithVaddr(void* vaddr, size_t size) {
  EccHardwareBuffer buf;
  buf.vaddr = vaddr;
  buf.ion_fd = -1;
  buf.size = size;
  return buf;
}

const void* EccGetMessageBody(EccMessageHandle message_handle) {
  return get_message(message_handle)->getBody();
}

size_t EccGetMessageBodySize(EccMessageHandle message_handle) {
  return get_message(message_handle)->getBodySize();
}

void EccCreate(EccMode mode, EccHandle* ecc_client_handle_ptr) {
  auto end_point =
      EaselComm2::Comm::create(ecc_to_EaselComm2_mode(mode)).release();
  *ecc_client_handle_ptr = reinterpret_cast<EccHandle>(end_point);
}

int EccOpen(EccHandle ecc_handle, EccServiceId service_id) {
  return get_client(ecc_handle)->open(ecc_service_to_easel_service(service_id));
}

int EccOpenPersistent(EccHandle ecc_handle, EccServiceId service_id) {
  return get_client(ecc_handle)->openPersistent(
      ecc_service_to_easel_service(service_id));
}

void EccClose(EccHandle ecc_handle) {
  get_client(ecc_handle)->close();
  delete get_client(ecc_handle);
}

int EccStartReceiving(EccHandle ecc_handle) {
  return get_client(ecc_handle)->startReceiving();
}

void EccJoinReceiving(EccHandle ecc_handle) {
  get_client(ecc_handle)->joinReceiving();
}

int EccSendWithPayload(EccHandle ecc_handle, int channel_id, const void* body,
                       size_t body_size, EccHardwareBuffer payload) {
  auto hardware_buffer = get_hardware_buffer(payload);
  return get_client(ecc_handle)
      ->send(channel_id, body, body_size, &hardware_buffer);
}

int EccSend(EccHandle ecc_handle, int channel_id, const void* body,
            size_t body_size) {
  return get_client(ecc_handle)->send(channel_id, body, body_size);
}

void EccRegisterHandler(EccHandle ecc_handle, int channel_id,
                        EccHandler ecc_handler, void* user_data) {
  get_client(ecc_handle)
      ->registerHandler(channel_id, [ecc_handler, user_data,
                                     channel_id](const EaselComm2::Message& m) {
        auto message_handle = get_message_handle(m);
        (*ecc_handler)(channel_id, message_handle, user_data);
      });
}

int EccReceivePayload(EccHandle ecc_handle, EccMessageHandle message,
                      EccHardwareBuffer buffer) {
  auto hardware_buffer = get_hardware_buffer(buffer);
  return get_client(ecc_handle)
      ->receivePayload(*get_message(message), &hardware_buffer);
}

