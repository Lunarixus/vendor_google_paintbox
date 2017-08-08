#define LOG_TAG "nnserver"

#include "EaselComm2.h"
#include "Rpc.h"
#include "log/log.h"
#include "vendor/google_paintbox/nn/shared/proto/types.pb.h"

namespace paintbox_nn {

void handlePrepareModel(const EaselComm2::Message& message) {
  ALOGI("received PrepareModel");
  Model model;
  bool success = message.toProto(&model);
  if (!success) {
    ALOGE("Model invalid");
    return;
  }
  ALOGI("model size %d\n", static_cast<int>(model.ByteSize()));
}

void handleExecute(const EaselComm2::Message& message) {
  ALOGI("received Execute");
  Request request;
  bool success = message.toProto(&request);
  if (!success) {
    ALOGE("Request invalid");
    return;
  }
  ALOGI("request size %d\n", static_cast<int>(request.ByteSize()));
}

}  // paintbox_nn

int main() {
  auto comm = EaselComm2::Comm::create(EaselComm2::Comm::Mode::SERVER);
  comm->registerHandler(PREPARE_MODEL, paintbox_nn::handlePrepareModel);
  comm->registerHandler(EXECUTE, paintbox_nn::handleExecute);
  while (true) {
    comm->open(EASEL_SERVICE_NN);
    comm->startReceiving();
    comm->joinReceiving();
    comm->close();
  }
}
