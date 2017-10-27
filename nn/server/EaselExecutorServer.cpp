#define LOG_TAG "EaselExecutorServer"

#include "EaselExecutorServer.h"
#include "Rpc.h"

#include <android-base/logging.h>

namespace paintbox_nn {

EaselExecutorServer::EaselExecutorServer() {
  mComm = EaselComm2::Comm::create(EaselComm2::Comm::Mode::SERVER);
  mState = State::INIT;
}

void EaselExecutorServer::start() {
  CHECK(mState == State::INIT);

  mComm->registerHandler(
      PREPARE_MODEL,
      [&](const EaselComm2::Message& message) { handlePrepareModel(message); });

  mComm->registerHandler(
      DESTROY_MODEL,
      [&](const EaselComm2::Message& message) { handleDestroyModel(message); });

  mComm->registerHandler(EXECUTE, [&](const EaselComm2::Message& message) {
    handleExecute(message);
  });

  mComm->openPersistent(EASEL_SERVICE_NN);
}

void EaselExecutorServer::handlePrepareModel(
    const EaselComm2::Message& message) {
  LOG(INFO) << "received PrepareModel";

  std::lock_guard<std::mutex> lock(mExecutorLock);

  if (!message.hasPayload()) {
    // If it is a message with Model object.
    // Should be in destroyed state.
    CHECK(mState == State::INIT || mState == State::MODEL_DESTROYED);
    CHECK(mModel.pools.empty());
    CHECK(!mExecutorThread.joinable());
    CHECK(message.toProto(&mModel.model));
    LOG(INFO) << "PrepareModel done. model size " << mModel.model.ByteSize()
              << " pool size " << mModel.model.poolsizes().size();
    mState = State::MODEL_RECEIVED;
    // If model does not have pools, set to fully received state right away.
    if (mModel.model.poolsizes().empty()) {
      modelFullyReceived();
    }
  } else {
    CHECK(mState == State::MODEL_RECEIVED);

    // If it is a message with pool that comes after Model:
    int id = message.getPayload().id();

    CHECK_LT(id, mModel.model.poolsizes().size());
    // Check we have received the previous buffers.
    CHECK_EQ(mModel.pools.size(), static_cast<size_t>(id));
    size_t inputSize = message.getPayload().size();
    CHECK_EQ(inputSize, mModel.model.poolsizes(id));

    // Receive input data.
    EaselComm2::HardwareBuffer hardwareBuffer(inputSize, id);
    CHECK(hardwareBuffer.valid());
    CHECK_EQ(mComm->receivePayload(message, &hardwareBuffer), 0);
    mModel.pools.push_back(hardwareBuffer);

    // Send the response on the last buffer.
    if (id == mModel.model.poolsizes().size() - 1) {
      modelFullyReceived();
    }
  }
}

void EaselExecutorServer::modelFullyReceived() {
  mState = State::MODEL_POOLS_RECEIVED;
  PrepareModelResponse response;
  response.set_error(NONE);
  mComm->send(PREPARE_MODEL, response);

  // Start the executor thread.
  mExecutorThread = std::thread([&] { executeRunThread(); });
}

void EaselExecutorServer::handleExecute(const EaselComm2::Message& message) {
  LOG(INFO) << "received Execute";
  std::lock_guard<std::mutex> lock(mExecutorLock);

  if (!message.hasPayload()) {
    CHECK(mState == State::MODEL_POOLS_RECEIVED ||
          mState == State::REQUEST_POOLS_RECEIVED);

    RequestPair pair;
    CHECK(message.toProto(&pair.request));
    LOG(INFO) << "request size " << pair.request.ByteSize() << " pool size "
              << pair.request.poolsizes().size();
    CHECK(pair.pools.empty());
    pair.pools.resize(pair.request.poolsizes().size());
    mRequests.emplace(pair);
    mState = State::REQUEST_RECEIVED;

    // If request does not need input pools, set to fully received state right
    // away.
    if (pair.request.inputpools().empty()) {
      requestFullyReceived();
    }

  } else {
    CHECK(mState == State::REQUEST_RECEIVED);

    auto& request = mRequests.back().request;
    auto& pools = mRequests.back().pools;
    // id is also the index of pools (not inputpools or outputpools).
    int id = message.getPayload().id();

    CHECK_LT(id, static_cast<int>(request.poolsizes().size()));
    // TODO(cjluo): find a efficient way to check
    // 1) id is inside request.pools().
    // 2) we have received the previous buffers.

    size_t inputSize = message.getPayload().size();
    CHECK_EQ(inputSize, request.poolsizes(id));

    EaselComm2::HardwareBuffer hardwareBuffer(inputSize);
    CHECK(hardwareBuffer.valid());
    CHECK_EQ(mComm->receivePayload(message, &hardwareBuffer), 0);
    pools[id] = hardwareBuffer;

    // Set to request fully received state on last input buffer.
    int last = request.inputpools().size() - 1;
    if (id == request.inputpools(last)) {
      requestFullyReceived();
    }
  }
}

void EaselExecutorServer::requestFullyReceived() {
  mState = State::REQUEST_POOLS_RECEIVED;
  mRequestAvailable.notify_one();
}

void EaselExecutorServer::executeRunThread() {
  std::unique_lock<std::mutex> lock(mExecutorLock);
  while (true) {
    mRequestAvailable.wait(lock, [&] {
      return !mRequests.empty() || mState == State::MODEL_DESTROYING;
    });
    if (mState == State::MODEL_DESTROYING) {
      LOG(INFO) << "Model about to be destroyed, finishing executor thread.";
      while (!mRequests.empty()) {
        mRequests.pop();
      }
      return;
    }

    RequestPair& pair = mRequests.front();
    // Allocate empty output pools.
    for (size_t i = 0; i < pair.pools.size(); i++) {
      if (!pair.pools[i].valid()) {
        size_t size = pair.request.poolsizes(i);
        pair.pools[i] = EaselComm2::HardwareBuffer(size);
        CHECK(pair.pools[i].valid());
      }
    }

    // TODO(cjluo): implement this thread function

    std::vector<EaselComm2::HardwareBuffer> hardwareBuffers;
    // Send output pools back to client.
    for (int outputIndex : pair.request.outputpools()) {
      pair.pools[outputIndex].setId(outputIndex);
      hardwareBuffers.push_back(pair.pools[outputIndex]);
    }
    mComm->send(EXECUTE, hardwareBuffers);
    RequestResponse response;
    response.set_error(NONE);
    mComm->send(EXECUTE, response);

    // Release allocated resource
    mRequests.pop();
  }
}

void EaselExecutorServer::handleDestroyModel(const EaselComm2::Message&) {
  LOG(INFO) << "received DestoryModel";

  {
    std::lock_guard<std::mutex> lock(mExecutorLock);
    CHECK(mState != State::INIT && mState != State::MODEL_DESTROYING &&
          mState != State::MODEL_DESTROYED);
    mState = State::MODEL_DESTROYING;
  }

  mRequestAvailable.notify_one();

  if (mExecutorThread.joinable()) {
    mExecutorThread.join();
  }

  std::lock_guard<std::mutex> lock(mExecutorLock);
  mModel.model = Model();
  mModel.pools.clear();

  TearDownModelResponse response;
  response.set_error(NONE);
  mComm->send(DESTROY_MODEL, response);
  mState = State::MODEL_DESTROYED;
}

}  // namespace paintbox_nn
