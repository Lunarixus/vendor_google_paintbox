#define LOG_TAG "EaselExecutorClient"

#include "EaselExecutorClient.h"

#include "Rpc.h"

#include "vendor/google_paintbox/blue/nn/shared/proto/types.pb.h"

#include <android-base/logging.h>

namespace android {
namespace nn {
namespace paintbox_driver {

EaselExecutorClient::EaselExecutorClient() {
  mComm = easel::CreateComm(easel::Comm::Type::CLIENT);
  mPrepareModelHandler = std::make_unique<easel::FunctionHandler>(
      [this](const easel::Message& message) {
        this->prepareModelHandler(message);
      });
  mExecuteHandler = std::make_unique<easel::FunctionHandler>(
      [this](const easel::Message& message) { this->executeHandler(message); });
  mDestroyModelHandler = std::make_unique<easel::FunctionHandler>(
      [this](const easel::Message& message) {
        this->destroyModelHandler(message);
      });
}

EaselExecutorClient::~EaselExecutorClient() { mComm->Close(); }

int EaselExecutorClient::initialize() {
  LOG(DEBUG) << __FUNCTION__;

  std::lock_guard<std::mutex> lock(mExecutorLock);
  CHECK(mRequestQueue.empty());

  int res = mComm->Open(easel::EASEL_SERVICE_NN);
  if (res != 0) return res;

  // Register the prepare model handler.
  mComm->RegisterHandler(PREPARE_MODEL, mPrepareModelHandler.get());

  // Register the execute handler.
  mComm->RegisterHandler(EXECUTE, mExecuteHandler.get());

  // Register the destroy handler.
  mComm->RegisterHandler(DESTROY_MODEL, mDestroyModelHandler.get());

  res = mComm->StartReceiving();
  return res;
}

int EaselExecutorClient::prepareModel(
    const Model& model, int64_t modelId,
    std::function<void(const paintbox_nn::PrepareModelResponse&)> callback) {
  LOG(DEBUG) << __FUNCTION__;

  std::unique_lock<std::mutex> lock(mExecutorLock);

  // Update mModels.
  ModelObject modelObject;
  modelObject.model = &model;
  modelObject.callback = callback;
  modelObject.bufferPools =
      std::vector<paintbox_util::HardwareBufferPool>(model.pools.size());

  paintbox_nn::Model protoModel;
  paintbox_util::convertHidlModel(model, modelId, &protoModel);

  // Prepare the buffer pools to be sent to Easel.
  for (size_t i = 0; i < model.pools.size(); i++) {
    auto& pool = model.pools[i];
    paintbox_util::HardwareBufferPool bufferPool;
    CHECK(paintbox_util::mapPool(pool, &bufferPool));
    bufferPool.buffer->SetId(i);
    modelObject.bufferPools[i] = std::move(bufferPool);
  }

  // Send the model object first.
  int res = easel::SendProto(mComm.get(), PREPARE_MODEL, protoModel,
                             /*payload=*/nullptr);
  if (res != 0) return res;

  // Then send the buffer pools.
  if (!modelObject.bufferPools.empty()) {
    for (paintbox_util::HardwareBufferPool& bufferPool :
         modelObject.bufferPools) {
      paintbox_nn::ModelPoolRequest request;
      request.set_modelid(modelId);
      res = easel::SendProto(mComm.get(), PREPARE_MODEL, request,
                             bufferPool.buffer.get());
      if (res != 0) {
        LOG(ERROR) << "Failed to send model pool, return code " << res;
        return res;
      }
    }
  }

  // Update mModels.
  CHECK(mModels.find(modelId) == mModels.end());
  mModels.emplace(modelId, std::move(modelObject));
  mModelsChanged.notify_all();

  return 0;
}

void EaselExecutorClient::prepareModelHandler(const easel::Message& message) {
  LOG(DEBUG) << __FUNCTION__;
  paintbox_nn::PrepareModelResponse response;
  CHECK(easel::MessageToProto(message, &response));

  std::lock_guard<std::mutex> lock(mExecutorLock);
  auto iter = mModels.find(response.modelid());
  CHECK(iter != mModels.end());

  auto& modelObject = iter->second;
  // Update the state and callback to driver about preparation result.
  modelObject.callback(response);
  // Clears the callback to release the PaintboxPrepareModel reference count.
  modelObject.callback = nullptr;
}

int EaselExecutorClient::execute(
    const Request& request, int64_t modelId,
    std::function<void(const paintbox_nn::RequestResponse&)> callback) {
  LOG(DEBUG) << __FUNCTION__;

  std::unique_lock<std::mutex> lock(mExecutorLock);
  // Model should be prepared before execute.
  CHECK(mModels.find(modelId) != mModels.end());

  paintbox_nn::Request protoRequest;
  paintbox_util::convertHidlRequest(request, modelId, &protoRequest);

  // Update the request queue.
  mRequestQueue.push(
      {&request, callback,
       std::vector<paintbox_util::HardwareBufferPool>(request.pools.size())});

  RequestObject& object = mRequestQueue.back();

  for (size_t i = 0; i < request.pools.size(); i++) {
    auto& pool = request.pools[i];
    paintbox_util::HardwareBufferPool bufferPool;
    CHECK(paintbox_util::mapPool(pool, &bufferPool));
    bufferPool.buffer->SetId(i);
    object.bufferPools[i] = std::move(bufferPool);
  }

  // Send the request object first.
  int res =
      easel::SendProto(mComm.get(), EXECUTE, protoRequest, /*payload=*/nullptr);
  if (res != 0) return res;

  // Then send the buffer pools.
  for (int i = 0; i < protoRequest.inputpools().size(); i++) {
    int poolIndex = protoRequest.inputpools(i);
    res = mComm->Send(EXECUTE, object.bufferPools[poolIndex].buffer.get());
    if (res != 0) {
      LOG(ERROR) << "Failed to send request pool, return code " << res;
      return res;
    }
  }

  return 0;
}

void EaselExecutorClient::executeHandler(const easel::Message& message) {
  LOG(DEBUG) << __FUNCTION__;
  std::lock_guard<std::mutex> lock(mExecutorLock);

  if (message.GetPayloadSize() > 0) {
    // Updates the output buffer pools with results.
    int poolId = message.GetPayloadId();
    // TODO(cjluo): need to handle the return value of ReceivePayload.
    CHECK_EQ(
        mComm->ReceivePayload(
            message, mRequestQueue.front().bufferPools[poolId].buffer.get()),
        0);
    // TODO(cjluo): need to check how many output buffer gets returned.
  } else {
    // Request execution Callback
    paintbox_nn::RequestResponse response;
    CHECK(easel::MessageToProto(message, &response));

    // Pop the finished request from mRequestQueue.
    auto callback = mRequestQueue.front().callback;
    mRequestQueue.pop();
    callback(response);
  }
}

void EaselExecutorClient::destroyModel(int64_t modelId) {
  LOG(DEBUG) << __FUNCTION__;

  std::unique_lock<std::mutex> lock(mExecutorLock);

  paintbox_nn::DestroyModelRequest request;
  request.set_modelid(modelId);

  auto iter = mModels.find(modelId);
  if (iter == mModels.end()) {
    LOG(WARNING) << __FUNCTION__ << " model with ID" << modelId
                 << " has already been deleted.";
    return;
  }
  mModels.erase(iter);
  mModelsChanged.notify_all();

  CHECK_EQ(easel::SendProto(mComm.get(), DESTROY_MODEL, request,
                            /*payload=*/nullptr),
           0);
}

void EaselExecutorClient::destroyModelHandler(const easel::Message& message) {
  LOG(DEBUG) << __FUNCTION__;
  paintbox_nn::DestroyModelResponse response;
  CHECK(easel::MessageToProto(message, &response));

  if (response.error() != paintbox_nn::NONE) {
    LOG(ERROR) << "Could not delete model, id=" << response.modelid()
               << " error " << response.error();
  }
}

}  // namespace paintbox_driver
}  // namespace nn
}  // namespace android
