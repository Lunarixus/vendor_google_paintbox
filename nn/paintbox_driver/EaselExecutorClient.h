#ifndef PAINTBOX_EASEL_EXECUTOR_CLIENT_H
#define PAINTBOX_EASEL_EXECUTOR_CLIENT_H

#include <queue>

#include "Conversion.h"
#include "HalInterfaces.h"
#include "hardware/gchips/paintbox/system/include/easel_comm.h"
#include "hardware/gchips/paintbox/system/include/easel_comm_helper.h"
#include "vendor/google_paintbox/nn/shared/proto/types.pb.h"

namespace android {
namespace nn {
namespace paintbox_driver {

// Client of Easel executor.
// It forwards the model and requests to Easel for NN execution.
// Currently this client only support single outstanding model.
// TODO(cjluo): investigate supporting multiple model.
// E.g. create an ExecutorFactory and create individual executor for each model.
class EaselExecutorClient {
 public:
  EaselExecutorClient();
  ~EaselExecutorClient();

  // Initializes the client, sets up the message callback and establish the
  // communication channel. Prerequisite: INIT state, mRequestQueue empty.
  // Returns 0 if successful, otherwise the error code.
  int initialize();

  // Sends the model to Easel and run callback when finished.
  // Sets the mState to PREPARING during waiting easel response.
  // Sets the mState to PREPARED after receiving easel response.
  // Prerequisite: INIT or DESTROYED state, mRequestQueue empty.
  // Returns 0 if successful, otherwise the error code.
  int prepareModel(
      const Model& model,
      std::function<void(const paintbox_nn::PrepareModelResponse&)> callback);

  // Sends the execute request to Easel and run callback when finished.
  // Prerequisite: PREPARED state.
  // Returns 0 if successful, otherwise the error code.
  int execute(
      const Request& request,
      std::function<void(const paintbox_nn::RequestResponse&)> callback);

  // Signals Easel to destroy the prepared model.
  // Sets the mState to DESTROYING during waiting easel response.
  // Sets the mState to DESTROYED after receiving easel response.
  // Prerequisite: PREPARED state, mRequestQueue empty.
  void destroyModel(const Model& model);

 private:
  // Model object with related callback and bufferPools.
  struct ModelObject {
    const Model* model;
    std::function<void(const paintbox_nn::PrepareModelResponse&)> callback;
    std::vector<paintbox_util::HardwareBufferPool> bufferPools;
  };

  // Request paired with related callback and bufferPools.
  struct RequestObject {
    const Request* request;
    std::function<void(const paintbox_nn::RequestResponse&)> callback;
    std::vector<paintbox_util::HardwareBufferPool> bufferPools;
  };

  // State of EaselExecutor
  enum class State {
    INIT,        // Fresh start.
    INITED,      // Finished initialization.
    PREPARING,   // Model sent to Easel.
    PREPARED,    // Easel finishes the model preparation, ready for execution.
    DESTROYING,  // Model is about to be destoryed on Easel.
    DESTROYED,   // Model destroyed.
  };

  // EaselMessage handlers.
  void prepareModelHandler(const easel::Message& message);
  void executeHandler(const easel::Message& message);
  void destroyModelHandler(const easel::Message& message);

  std::unique_ptr<easel::Comm> mComm;
  std::mutex mExecutorLock;
  std::condition_variable mStateChanged;
  State mState;                             // Guarded by mExecutorLock.
  std::unique_ptr<ModelObject> mModel;      // Guarded by mExecutorLock.
  std::queue<RequestObject> mRequestQueue;  // Guarded by mExecutorLock.

  std::unique_ptr<easel::FunctionHandler> mPrepareModelHandler;
  std::unique_ptr<easel::FunctionHandler> mExecuteHandler;
  std::unique_ptr<easel::FunctionHandler> mDestroyModelHandler;
};

}  // namespace paintbox_driver
}  // namespace nn
}  // namespace android

#endif  // PAINTBOX_EASEL_EXECUTOR_CLIENT_H
