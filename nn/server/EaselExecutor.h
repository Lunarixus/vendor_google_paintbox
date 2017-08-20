#ifndef PAINTBOX_NN_EASEL_EXECUTOR_H
#define PAINTBOX_NN_EASEL_EXECUTOR_H

#include "EaselComm2.h"
#include "vendor/google_paintbox/nn/shared/proto/types.pb.h"

namespace paintbox_nn {

// A NN Request executor that executes requests from AP.
// The NN model execution is running on A53 CPU for now.
class EaselExecutor {
 public:
  EaselExecutor();
  // Starts handling models and requests from AP.
  // This function opens the PCIE link and registeres RPC handlers.
  // It will run as an infinite-loop until process ends.
  void start();

 private:
  // Handles PREPARE_MODEL request from AP.
  // Saves the model from message into mModel.
  void handlePrepareModel(const EaselComm2::Message& message);
  // Handles EXECUTE request from AP.
  // Retrieves the input buffer from AP from message, calculates the result and
  // sends the output buffer back.
  void handleExecute(const EaselComm2::Message& message);
  std::unique_ptr<EaselComm2::Comm> mComm;
  Model mModel;
  std::atomic<bool> mPrepared;
};

}  // namespace paintbox_nn

#endif  // PAINTBOX_NN_EASEL_EXECUTOR_H
