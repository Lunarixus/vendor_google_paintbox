#ifndef PAINTBOX_EASEL_EXECUTOR_CLIENT_H
#define PAINTBOX_EASEL_EXECUTOR_CLIENT_H

#include "EaselComm2.h"
#include "HalInterfaces.h"
#include "vendor/google_paintbox/nn/shared/proto/types.pb.h"

#include <queue>

namespace android {
namespace nn {
namespace paintbox_driver {

// Model paired with related callback.
struct ModelPair {
  const Model* model;
  std::function<void(const paintbox_nn::PrepareModelResponse&)> prepareCallback;
  std::function<void(const paintbox_nn::TearDownModelResponse&)>
      destroyCallback;
};

// Request paired with related callback.
struct RequestPair {
  const Request* request;
  std::function<void(const paintbox_nn::RequestResponse&)> callback;
};

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

  // Signals the execute request to Easel and run callback when finished.
  // Sets the mState to DESTROYING during waiting easel response.
  // Sets the mState to DESTROYED after receiving easel response.
  // Prerequisite: PREPARED state, mRequestQueue empty.
  // Returns 0 if successful, otherwise the error code.
  int destroyModel(
      const Model& model,
      std::function<void(const paintbox_nn::TearDownModelResponse&)> callback);

 private:
  // State of EaselExecutor
  enum class State {
    INIT,        // Fresh start.
    PREPARING,   // Model sent to Easel.
    PREPARED,    // Easel finishes the model preparation, ready for execution.
    DESTROYING,  // Model is about to be destoryed on Easel.
    DESTROYED,   // Model destroyed.
  };

  // EaselMessage handlers.
  void prepareModelHandler(const EaselComm2::Message& message);
  void executeHandler(const EaselComm2::Message& message);
  void destroyModelHandler(const EaselComm2::Message& message);

  std::unique_ptr<EaselComm2::Comm> mComm;
  std::mutex mExecutorLock;
  std::condition_variable mStateChanged;
  State mState;                           // Guarded by mExecutorLock.
  std::unique_ptr<ModelPair> mModel;      // Guarded by mExecutorLock.
  std::queue<RequestPair> mRequestQueue;  // Guarded by mExecutorLock.
};

}  // namespace paintbox_driver
}  // namespace nn
}  // namespace android

#endif  // PAINTBOX_EASEL_EXECUTOR_CLIENT_H
