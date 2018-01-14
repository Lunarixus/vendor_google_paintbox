#ifndef PAINTBOX_EASEL_EXECUTOR_CLIENT_H
#define PAINTBOX_EASEL_EXECUTOR_CLIENT_H

#include <queue>
#include <unordered_map>

#include "Conversion.h"
#include "HalInterfaces.h"
#include "hardware/gchips/paintbox/system/include/easel_comm.h"
#include "hardware/gchips/paintbox/system/include/easel_comm_helper.h"
#include "vendor/google_paintbox/blue/nn/shared/proto/types.pb.h"

namespace android {
namespace nn {
namespace paintbox_driver {

// Client of Easel executor.
// It forwards the model and requests to Easel for NN execution.
// E.g. create an ExecutorFactory and create individual executor for each model.
class EaselExecutorClient {
 public:
  EaselExecutorClient();
  ~EaselExecutorClient();

  // Initializes the client, sets up the message callback and establish the
  // communication channel.
  // Should only be called once.
  // Returns 0 if successful, otherwise the error code.
  int initialize();

  // Sends the model to Easel and run callback when finished.
  // Returns 0 if successful, otherwise the error code.
  int prepareModel(
      const Model& model, int64_t modelId,
      std::function<void(const paintbox_nn::PrepareModelResponse&)> callback);

  // Sends the execute request to Easel and run callback when finished.
  // Returns 0 if successful, otherwise the error code.
  int execute(
      const Request& request, int64_t modelId,
      std::function<void(const paintbox_nn::RequestResponse&)> callback);

  // Signals Easel to destroy the prepared model.
  void destroyModel(int64_t modelId);

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

  // EaselMessage handlers.
  void prepareModelHandler(const easel::Message& message);
  void executeHandler(const easel::Message& message);
  void destroyModelHandler(const easel::Message& message);

  std::unique_ptr<easel::Comm> mComm;
  std::mutex mExecutorLock;
  std::unordered_map<int64_t, ModelObject>
      mModels;                              // Guarded by mExecutorLock.
  std::queue<RequestObject> mRequestQueue;  // Guarded by mExecutorLock.
  std::condition_variable mModelsChanged;

  std::unique_ptr<easel::FunctionHandler> mPrepareModelHandler;
  std::unique_ptr<easel::FunctionHandler> mExecuteHandler;
  std::unique_ptr<easel::FunctionHandler> mDestroyModelHandler;
};

}  // namespace paintbox_driver
}  // namespace nn
}  // namespace android

#endif  // PAINTBOX_EASEL_EXECUTOR_CLIENT_H
