#define LOG_TAG "EaselExecutorClient"

#include "EaselExecutorClient.h"

#include "Conversion.h"
#include "Rpc.h"

#include <android-base/logging.h>

namespace android {
namespace nn {
namespace paintbox_driver {

EaselExecutorClient::EaselExecutorClient() : mState(State::INIT) {
  mComm = EaselComm2::Comm::create(EaselComm2::Comm::Mode::CLIENT);
}

EaselExecutorClient::~EaselExecutorClient() { mComm->close(); }

int EaselExecutorClient::initialize() {
  CHECK(mState == State::INIT);
  CHECK(mRequestQueue.empty());

  int res = mComm->open(EASEL_SERVICE_NN);
  if (res != 0) return res;

  // Register the prepare model handler.
  mComm->registerHandler(PREPARE_MODEL,
                         [&](const EaselComm2::Message& message) {
                           prepareModelHandler(message);
                         });

  // Register the execute handler.
  mComm->registerHandler(EXECUTE, [&](const EaselComm2::Message& message) {
    executeHandler(message);
  });

  // Register the tear down handler.
  mComm->registerHandler(DESTROY_MODEL,
                         [&](const EaselComm2::Message& message) {
                           destroyModelHandler(message);
                         });

  res = mComm->startReceiving();
  return res;
}

int EaselExecutorClient::prepareModel(
    const Model& model,
    std::function<void(const paintbox_nn::PrepareModelResponse&)> callback) {
  {
    // Wait for readiness.
    std::unique_lock<std::mutex> lock(mExecutorLock);
    mStateChanged.wait(lock, [&] {
      return ((mState == State::INIT) || (mState == State::DESTROYED)) &&
             (mModel == nullptr) && mRequestQueue.empty();
    });

    // Update mModel and mState.
    mModel = std::make_unique<ModelPair>();
    mModel->model = &model;
    mModel->callback = callback;
    mState = State::PREPARING;
  }

  paintbox_nn::Model protoModel;
  paintbox_util::convertHidlModel(model, &protoModel);

  // Prepare the buffer pools to be sent to Easel.
  std::vector<EaselComm2::HardwareBuffer> pools(model.pools.size());
  for (size_t i = 0; i < model.pools.size(); i++) {
    auto& pool = model.pools[i];
    EaselComm2::HardwareBuffer buffer;
    CHECK(paintbox_util::mapPool(pool, &buffer));
    buffer.setId(i);
    pools[i] = buffer;
  }

  std::lock_guard<std::mutex> lock(mExecutorLock);
  // Send the model object first.
  int res = mComm->send(PREPARE_MODEL, protoModel);
  if (res != 0) return res;

  // Then send the buffer pools.
  return mComm->send(PREPARE_MODEL, pools);
}

void EaselExecutorClient::prepareModelHandler(
    const EaselComm2::Message& message) {
  paintbox_nn::PrepareModelResponse response;
  CHECK(message.toProto(&response));

  {
    std::lock_guard<std::mutex> lock(mExecutorLock);
    CHECK(mModel != nullptr);
    // TODO(cjluo): need to handle error.
    CHECK_EQ(response.error(), paintbox_nn::NONE);

    // Update the state and callback to driver about preparation result.
    mState = State::PREPARED;
    mModel->callback(response);
  }

  mStateChanged.notify_all();
}

int EaselExecutorClient::execute(
    const Request& request,
    std::function<void(const paintbox_nn::RequestResponse&)> callback) {
  {
    // Wait for readiness.
    std::unique_lock<std::mutex> lock(mExecutorLock);
    mStateChanged.wait(lock, [&] {
      return (mState == State::PREPARED) && (mModel != nullptr);
    });
  }

  paintbox_nn::Request protoRequest;
  paintbox_util::convertHidlRequest(request, &protoRequest);

  // Prepare the buffer pools to be sent to Easel.
  // We only send input pools to Easel.
  std::vector<EaselComm2::HardwareBuffer> pools(
      protoRequest.inputpools().size());
  for (int i = 0; i < protoRequest.inputpools().size(); i++) {
    int poolIndex = protoRequest.inputpools(i);
    auto& pool = request.pools[poolIndex];
    EaselComm2::HardwareBuffer buffer;
    CHECK(paintbox_util::mapPool(pool, &buffer));
    buffer.setId(i);
    pools[i] = buffer;
  }

  // Updates the request queue.
  std::lock_guard<std::mutex> lock(mExecutorLock);
  CHECK(mModel != nullptr);
  mRequestQueue.push({&request, callback});

  // Send the request object first.
  int res = mComm->send(EXECUTE, protoRequest);
  if (res != 0) return res;

  // Then send the buffer pools.
  return mComm->send(EXECUTE, pools);
}

void EaselExecutorClient::executeHandler(const EaselComm2::Message& message) {
  if (message.hasPayload()) {
    // Updates the output buffer pools with results.
    int poolId = message.getHeader()->payloadId;
    std::lock_guard<std::mutex> lock(mExecutorLock);
    // Always assume the current request is on the front of the queue.
    auto& pool = mRequestQueue.front().request->pools[poolId];

    EaselComm2::HardwareBuffer buffer;
    CHECK(paintbox_util::mapPool(pool, &buffer));
    mComm->receivePayload(message, &buffer);
    // TODO(cjluo): need to check how many output buffer gets returned.
  } else {
    // Request execution Callback
    paintbox_nn::RequestResponse response;
    CHECK(message.toProto(&response));

    {
      // Pop the finished request from mRequestQueue.
      std::lock_guard<std::mutex> lock(mExecutorLock);
      auto callback = mRequestQueue.front().callback;
      mRequestQueue.pop();
      callback(response);
    }

    if (mRequestQueue.empty()) mStateChanged.notify_all();
  }
}

void EaselExecutorClient::destroyModel(const Model& model) {
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

  mComm->send(DESTROY_MODEL);

  // Wait until server acknowledges the model to be destroyed.
  mStateChanged.wait(lock, [&] { return mState == State::DESTROYED; });
}

void EaselExecutorClient::destroyModelHandler(
    const EaselComm2::Message& message) {
  paintbox_nn::TearDownModelResponse response;
  CHECK(message.toProto(&response));

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
