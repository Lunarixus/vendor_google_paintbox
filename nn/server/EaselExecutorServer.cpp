#define LOG_TAG "EaselExecutorServer"

#include "CpuExecutor.h"
#include "EaselExecutorServer.h"
#include "Rpc.h"

#include <log/log.h>

namespace paintbox_nn {

EaselExecutorServer::EaselExecutorServer() {
  mComm = std::unique_ptr<easel::Comm>(
      easel::Comm::Create(easel::Comm::Type::SERVER));
  mState = State::INIT;

  mPrepareModelHandler = std::make_unique<easel::FunctionHandler>(
      [this](const easel::Message& message) { handlePrepareModel(message); });

  mExecuteHandler = std::make_unique<easel::FunctionHandler>(
      [this](const easel::Message& message) { handleExecute(message); });

  mDestroyModelHandler = std::make_unique<easel::FunctionHandler>(
      [this](const easel::Message& message) { handleDestroyModel(message); });
}

void EaselExecutorServer::start() {
  CHECK(mState == State::INIT);

  mComm->RegisterHandler(PREPARE_MODEL, mPrepareModelHandler.get());
  mComm->RegisterHandler(EXECUTE, mExecuteHandler.get());
  mComm->RegisterHandler(DESTROY_MODEL, mDestroyModelHandler.get());
  mComm->OpenPersistent(easel::EASEL_SERVICE_NN, 0);
}

void EaselExecutorServer::handlePrepareModel(const easel::Message& message) {
  ALOGI("received PrepareModel");

  std::lock_guard<std::mutex> lock(mExecutorLock);

  if (message.GetPayloadSize() <= 0) {
    // If it is a message with Model object.
    // Should be in destroyed state.
    CHECK(mState == State::INIT || mState == State::MODEL_DESTROYED);
    CHECK(mModel.pools.empty());
    CHECK(!mExecutorThread.joinable());
    CHECK(easel::MessageToProto(message, &mModel.model));
    ALOGI("PrepareModel done. model size %d pool size %d",
          mModel.model.ByteSize(), mModel.model.poolsizes().size());
    mState = State::MODEL_RECEIVED;
    // If model does not have pools, set to fully received state right away.
    if (mModel.model.poolsizes().empty()) {
      modelFullyReceived();
    }
  } else {
    CHECK(mState == State::MODEL_RECEIVED);

    // If it is a message with pool that comes after Model:
    int id = message.GetPayloadId();

    CHECK_LT(id, mModel.model.poolsizes().size());
    // Check we have received the previous buffers.
    CHECK_EQ(mModel.pools.size(), static_cast<size_t>(id));
    size_t inputSize = message.GetPayloadSize();
    CHECK_EQ(inputSize, mModel.model.poolsizes(id));

    // Receive input data.
    std::unique_ptr<easel::HardwareBuffer> hardwareBuffer =
        easel::AllocateHardwareBuffer(inputSize, id);
    CHECK(hardwareBuffer != nullptr);
    CHECK_EQ(mComm->ReceivePayload(message, hardwareBuffer.get()), 0);
    mModel.pools.push_back(std::move(hardwareBuffer));

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
  easel::SendProto(mComm.get(), PREPARE_MODEL, response, /*payload=*/nullptr);

  // Start the executor thread.
  mExecutorThread = std::thread([&] { executeRunThread(); });
}

void EaselExecutorServer::handleExecute(const easel::Message& message) {
  ALOGI("received Execute");
  std::lock_guard<std::mutex> lock(mExecutorLock);

  if (message.GetPayloadSize() <= 0) {
    CHECK(mState == State::MODEL_POOLS_RECEIVED ||
          mState == State::REQUEST_POOLS_RECEIVED);

    RequestPair pair;
    easel::MessageToProto(message, &pair.request);
    ALOGI("request size %d pool size %d", pair.request.ByteSize(),
          pair.request.poolsizes().size());
    CHECK(pair.pools.empty());
    pair.pools.resize(pair.request.poolsizes().size());
    mRequests.push(std::move(pair));
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
    int id = message.GetPayloadId();

    CHECK_LT(id, static_cast<int>(request.poolsizes().size()));
    // TODO(cjluo): find a efficient way to check
    // 1) id is inside request.pools().
    // 2) we have received the previous buffers.

    size_t inputSize = message.GetPayloadSize();
    CHECK_EQ(inputSize, request.poolsizes(id));

    std::unique_ptr<easel::HardwareBuffer> hardwareBuffer =
        easel::AllocateHardwareBuffer(inputSize);
    CHECK(hardwareBuffer->Valid());
    CHECK_EQ(mComm->ReceivePayload(message, hardwareBuffer.get()), 0);
    pools[id] = std::move(hardwareBuffer);

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

namespace {
void setRunTimePoolInfosFromHardwareBuffer(
    std::vector<android::nn::RunTimePoolInfo>* poolInfos,
    std::vector<std::unique_ptr<easel::HardwareBuffer>>* pools) {
  poolInfos->resize(pools->size());
  for (size_t i = 0; i < pools->size(); i++) {
    auto& poolInfo = (*poolInfos)[i];
    poolInfo.buffer = static_cast<uint8_t*>((*pools)[i]->GetVaddrMutable());
  }
}
}  // namespace

void EaselExecutorServer::executeRunThread() {
  std::unique_lock<std::mutex> lock(mExecutorLock);
  while (true) {
    mRequestAvailable.wait(lock, [&] {
      return !mRequests.empty() || mState == State::MODEL_DESTROYING;
    });
    if (mState == State::MODEL_DESTROYING) {
      ALOGI("Model about to be destroyed, finishing executor thread.");
      while (!mRequests.empty()) {
        mRequests.pop();
      }
      return;
    }

    RequestPair& pair = mRequests.front();
    // Allocate empty output pools.
    for (size_t i = 0; i < pair.pools.size(); i++) {
      if (pair.pools[i] == nullptr) {
        size_t size = pair.request.poolsizes(i);
        pair.pools[i] = easel::AllocateHardwareBuffer(size);
        CHECK(pair.pools[i]->Valid());
      }
    }

    std::vector<android::nn::RunTimePoolInfo> modelPoolInfos;
    setRunTimePoolInfosFromHardwareBuffer(&modelPoolInfos, &(mModel.pools));

    std::vector<android::nn::RunTimePoolInfo> requestPoolInfos;
    setRunTimePoolInfosFromHardwareBuffer(&requestPoolInfos, &(pair.pools));

    android::nn::CpuExecutor executor;
    int n = executor.run(mModel.model, pair.request, modelPoolInfos,
                         requestPoolInfos);
    ALOGI("executor.run returned %d", n);
    ErrorStatus executionStatus =
        n == ANEURALNETWORKS_NO_ERROR ? NONE : GENERAL_FAILURE;

    if (executionStatus == ErrorStatus::NONE) {
      // Send output pools back to client.
      for (int outputIndex : pair.request.outputpools()) {
        pair.pools[outputIndex]->SetId(outputIndex);
        mComm->Send(EXECUTE, pair.pools[outputIndex].get());
      }
    }

    RequestResponse response;
    response.set_error(executionStatus);
    SendProto(mComm.get(), EXECUTE, response, /*payload=*/nullptr);

    // Release allocated resource
    mRequests.pop();
  }
}

void EaselExecutorServer::handleDestroyModel(const easel::Message&) {
  ALOGI("received DestoryModel");

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
  easel::SendProto(mComm.get(), DESTROY_MODEL, response, /*payload=*/nullptr);
  mState = State::MODEL_DESTROYED;
}

}  // namespace paintbox_nn
