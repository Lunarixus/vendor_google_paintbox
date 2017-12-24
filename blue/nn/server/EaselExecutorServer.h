#ifndef PAINTBOX_NN_EASEL_EXECUTOR_SERVER_H
#define PAINTBOX_NN_EASEL_EXECUTOR_SERVER_H

#include "hardware/gchips/paintbox/system/include/easel_comm.h"
#include "hardware/gchips/paintbox/system/include/easel_comm_helper.h"
#include "vendor/google_paintbox/blue/nn/shared/proto/types.pb.h"

#include <mutex>
#include <queue>
#include <thread>

namespace paintbox_nn {

// A struct pair with model and related pools.
struct ModelPair {
  Model model;
  std::vector<std::unique_ptr<easel::HardwareBuffer>> pools;
};

// A struct pair with request and related pools.
struct RequestPair {
  Request request;
  std::vector<std::unique_ptr<easel::HardwareBuffer>> pools;
};

// A NN Request executor that executes requests from AP.
// The NN model execution is running on A53 CPU for now.
// Currently this server only supports a single outstanding model.
// TODO(cjluo): investigate supporting multiple model.
class EaselExecutorServer {
 public:
  EaselExecutorServer();

  // Starts handling models and requests from AP.
  // This function opens the PCIe link and registers RPC handlers.
  // It will run as an infinite-loop until process ends.
  void start();

 private:
  // Handles PREPARE_MODEL request from AP.
  // Saves the model and pools from message into mModel.
  void handlePrepareModel(const easel::Message& message);

  // Marks the state as model fully received.
  // Sets mState to MODEL_POOLS_RECEIVED.
  void modelFullyReceived();

  // Handles EXECUTE request from AP.
  // Retrieves the request object and input pool from AP and push to mRequests.
  // And signals mExecutorThread to process the request.
  void handleExecute(const easel::Message& message);

  // Marks the state as request fully received.
  // Sets mState to REQUEST_POOLS_RECEIVED.
  void requestFullyReceived();

  // Handles DESTROY_MODEL request from AP.
  // Resets mModel and releases allocated pools.
  void handleDestroyModel(const easel::Message& message);

  // Thread function that pulls request from mRequests and execute it in a loop.
  void executeRunThread();

  // State of EaselExecutor
  enum class State {
    INIT,
    MODEL_RECEIVED,
    MODEL_POOLS_RECEIVED,
    REQUEST_RECEIVED,
    REQUEST_POOLS_RECEIVED,
    MODEL_DESTROYING,
    MODEL_DESTROYED,
  };

  std::unique_ptr<easel::Comm> mComm;
  std::mutex mExecutorLock;
  std::condition_variable mRequestAvailable;
  ModelPair mModel;                   // Guarded by mExecutorLock.
  std::queue<RequestPair> mRequests;  // Guarded by mExecutorLock.
  State mState;                       // Guarded by mExecutorLock.
  std::thread mExecutorThread;

  std::unique_ptr<easel::FunctionHandler> mPrepareModelHandler;
  std::unique_ptr<easel::FunctionHandler> mExecuteHandler;
  std::unique_ptr<easel::FunctionHandler> mDestroyModelHandler;
};

}  // namespace paintbox_nn

#endif  // PAINTBOX_NN_EASEL_EXECUTOR_SERVER_H
