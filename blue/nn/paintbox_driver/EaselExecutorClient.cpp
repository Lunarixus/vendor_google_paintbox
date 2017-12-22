#define LOG_TAG "EaselExecutorClient"

#include "EaselExecutorClient.h"

#include "Rpc.h"

#include <android-base/logging.h>

namespace android {
namespace nn {
namespace paintbox_driver {

EaselExecutorClient::EaselExecutorClient() : mState(State::INIT) {
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
  if (mState != State::INIT) return 0;
  CHECK(mRequestQueue.empty());

  int res = mComm->Open(easel::EASEL_SERVICE_NN);
  if (res != 0) return res;

  // Register the prepare model handler.
  mComm->RegisterHandler(PREPARE_MODEL, mPrepareModelHandler.get());

  // Register the execute handler.
  mComm->RegisterHandler(EXECUTE, mExecuteHandler.get());

  // Register the tear down handler.
  mComm->RegisterHandler(DESTROY_MODEL, mDestroyModelHandler.get());

  res = mComm->StartReceiving();
  if (res == 0) mState = State::INITED;
  return res;
}

int EaselExecutorClient::prepareModel(
    const Model& model,
    std::function<void(const paintbox_nn::PrepareModelResponse&)> callback) {
  LOG(DEBUG) << __FUNCTION__;

  // Wait for readiness.
  std::unique_lock<std::mutex> lock(mExecutorLock);
  mStateChanged.wait(lock, [&] {
    return ((mState == State::INITED) || (mState == State::DESTROYED)) &&
           (mModel == nullptr) && mRequestQueue.empty();
  });

  // Update mModel and mState.
  mModel = std::make_unique<ModelObject>();
  mModel->model = &model;
  mModel->callback = callback;
  mModel->bufferPools =
      std::vector<paintbox_util::HardwareBufferPool>(model.pools.size());
  mState = State::PREPARING;

  paintbox_nn::Model protoModel;
  paintbox_util::convertHidlModel(model, &protoModel);

  // Prepare the buffer pools to be sent to Easel.
  for (size_t i = 0; i < model.pools.size(); i++) {
    auto& pool = model.pools[i];
    paintbox_util::HardwareBufferPool bufferPool;
    CHECK(paintbox_util::mapPool(pool, &bufferPool));
    bufferPool.buffer->SetId(i);
    mModel->bufferPools[i] = std::move(bufferPool);
  }

  // Send the model object first.
  int res = easel::SendProto(mComm.get(), PREPARE_MODEL, protoModel,
                             /*payload=*/nullptr);
  if (res != 0) return res;

  // Then send the buffer pools.
  if (!mModel->bufferPools.empty()) {
    for (paintbox_util::HardwareBufferPool& bufferPool : mModel->bufferPools) {
      res = mComm->Send(PREPARE_MODEL, bufferPool.buffer.get());
      if (res != 0) {
        LOG(ERROR) << "Failed to send model pool, return code " << res;
        return res;
      }
    }
  }
  return 0;
}

void EaselExecutorClient::prepareModelHandler(const easel::Message& message) {
  LOG(DEBUG) << __FUNCTION__;
  paintbox_nn::PrepareModelResponse response;
  CHECK(easel::MessageToProto(message, &response));

  {
    std::lock_guard<std::mutex> lock(mExecutorLock);
    CHECK(mModel != nullptr);
    // TODO(cjluo): need to handle error.
    CHECK_EQ(response.error(), paintbox_nn::NONE);

    // Update the state and callback to driver about preparation result.
    mState = State::PREPARED;
    mModel->callback(response);
    // Clears the callback to release the PaintboxPrepareModel reference count.
    mModel->callback = nullptr;
  }

  mStateChanged.notify_all();
}

int EaselExecutorClient::execute(
    const Request& request,
    std::function<void(const paintbox_nn::RequestResponse&)> callback) {
  LOG(DEBUG) << __FUNCTION__;

  // Wait for readiness.
  std::unique_lock<std::mutex> lock(mExecutorLock);
  mStateChanged.wait(
      lock, [&] { return (mState == State::PREPARED) && (mModel != nullptr); });

  paintbox_nn::Request protoRequest;
  paintbox_util::convertHidlRequest(request, &protoRequest);

  // Updates the request queue.
  CHECK(mModel != nullptr);
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
    mComm->ReceivePayload(
        message, mRequestQueue.front().bufferPools[poolId].buffer.get());
    // TODO(cjluo): need to check how many output buffer gets returned.
  } else {
    // Request execution Callback
    paintbox_nn::RequestResponse response;
    CHECK(easel::MessageToProto(message, &response));

    {
      // Pop the finished request from mRequestQueue.
      auto callback = mRequestQueue.front().callback;
      mRequestQueue.pop();
      callback(response);
    }

    if (mRequestQueue.empty()) mStateChanged.notify_all();
  }
}

void EaselExecutorClient::destroyModel(const Model& model) {
  LOG(DEBUG) << __FUNCTION__;
  // Wait for readiness.
  std::unique_lock<std::mutex> lock(mExecutorLock);
  mStateChanged.wait(lock, [&] {
    return (mState == State::PREPARED) && (mModel != nullptr) &&
           (mRequestQueue.empty());
  });

  CHECK_EQ(mModel->model, &model);
  CHECK(mRequestQueue.empty());

  // Update the state and destroy callback.
  mState = State::DESTROYING;

  mComm->Send(DESTROY_MODEL, /*payload=*/nullptr);

  // Wait until server acknowledges the model to be destroyed.
  mStateChanged.wait(lock, [&] { return mState == State::DESTROYED; });
}

void EaselExecutorClient::destroyModelHandler(const easel::Message& message) {
  LOG(DEBUG) << __FUNCTION__;
  paintbox_nn::TearDownModelResponse response;
  CHECK(easel::MessageToProto(message, &response));

  {
    // Update mModel, mState and make the callback.
    std::lock_guard<std::mutex> lock(mExecutorLock);
    CHECK(mModel != nullptr);
    // TODO(cjluo): need to handle error.
    CHECK_EQ(response.error(), paintbox_nn::NONE);
    mState = State::DESTROYED;
    mModel = nullptr;
  }

  mStateChanged.notify_all();
}

}  // namespace paintbox_driver
}  // namespace nn
}  // namespace android
