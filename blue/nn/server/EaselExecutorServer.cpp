#define LOG_TAG "EaselExecutorServer"

#include <thread>

#include "CpuExecutor.h"
#include "EaselExecutorServer.h"
#include "Rpc.h"

namespace paintbox_nn {

EaselExecutorServer::EaselExecutorServer() {
  mComm = std::unique_ptr<easel::Comm>(
      easel::Comm::Create(easel::Comm::Type::SERVER));

  mPrepareModelHandler = std::make_unique<easel::FunctionHandler>(
      [this](const easel::Message& message) { handlePrepareModel(message); });

  mExecuteHandler = std::make_unique<easel::FunctionHandler>(
      [this](const easel::Message& message) { handleExecute(message); });

  mDestroyModelHandler = std::make_unique<easel::FunctionHandler>(
      [this](const easel::Message& message) { handleDestroyModel(message); });
}

void EaselExecutorServer::start() {
  // Start the executor thread.
  std::thread([&] { executeRunThread(); }).detach();

  mComm->RegisterHandler(PREPARE_MODEL, mPrepareModelHandler.get());
  mComm->RegisterHandler(EXECUTE, mExecuteHandler.get());
  mComm->RegisterHandler(DESTROY_MODEL, mDestroyModelHandler.get());
  mComm->OpenPersistent(easel::EASEL_SERVICE_NN, 0);
}

void EaselExecutorServer::handlePrepareModel(const easel::Message& message) {
  std::lock_guard<std::mutex> lock(mExecutorLock);

  if (message.GetPayloadSize() <= 0) {
    // If it is a message with Model object.
    ModelPair modelPair;
    CHECK(easel::MessageToProto(message, &modelPair.model));
    Model& model = modelPair.model;
    int64_t modelId = model.modelid();
    bool noPools = model.poolsizes().empty();
    LOG(INFO) << "PrepareModel done. model size " << model.ByteSize()
              << " pool size " << model.poolsizes().size() << " model id "
              << modelId;

    // Save model to map
    auto iter = mModels.find(modelId);
    CHECK(iter == mModels.end());
    mModels.emplace(modelId, std::move(modelPair));

    // If model does not have pools, it is fully received right away.
    if (noPools) modelFullyReceived(modelId);
  } else {
    ModelPoolRequest request;
    CHECK(easel::MessageToProto(message, &request));
    int64_t modelId = request.modelid();

    // If it is a message with pool that comes after Model:
    int id = message.GetPayloadId();

    // Find the model from map.
    auto iter = mModels.find(modelId);
    CHECK(iter != mModels.end());
    ModelPair& modelPair = iter->second;

    CHECK_LT(id, modelPair.model.poolsizes().size());
    // Check we have received the previous buffers.
    CHECK_EQ(modelPair.pools.size(), static_cast<size_t>(id));
    size_t inputSize = message.GetPayloadSize();
    CHECK_EQ(inputSize, modelPair.model.poolsizes(id));

    // Receive input data.
    std::unique_ptr<easel::HardwareBuffer> hardwareBuffer =
        easel::AllocateHardwareBuffer(inputSize, id);
    CHECK(hardwareBuffer != nullptr);
    CHECK_EQ(mComm->ReceivePayload(message, hardwareBuffer.get()), 0);
    modelPair.pools.push_back(std::move(hardwareBuffer));

    // Send the response on the last buffer.
    if (modelPair.ready()) {
      modelFullyReceived(modelId);
    }
  }
}

void EaselExecutorServer::modelFullyReceived(int64_t modelId) {
  PrepareModelResponse response;
  response.set_error(NONE);
  response.set_modelid(modelId);
  CHECK_EQ(easel::SendProto(mComm.get(), PREPARE_MODEL, response,
                            /*payload=*/nullptr),
           0);
}

void EaselExecutorServer::handleExecute(const easel::Message& message) {
  std::lock_guard<std::mutex> lock(mExecutorLock);

  if (message.GetPayloadSize() <= 0) {
    RequestPair requestPair;
    easel::MessageToProto(message, &requestPair.request);
    bool noPools = requestPair.request.inputpools().empty();

    LOG(INFO) << "request size " << requestPair.request.ByteSize()
              << " pool size " << requestPair.request.poolsizes().size()
              << " model id " << requestPair.request.modelid();
    CHECK(requestPair.pools.empty());
    requestPair.pools.resize(requestPair.request.poolsizes().size());
    mRequests.push(std::move(requestPair));

    // If request does not need input pools, it is fully received right
    // away.
    if (noPools) requestFullyReceived();
  } else {
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
  mRequestAvailable.notify_one();
}

namespace {
// Constructs RuntimePoolInfo vector from HardwareBuffer pools.
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
    mRequestAvailable.wait(lock, [&] { return !mRequests.empty(); });

    RequestPair& requestPair = mRequests.front();
    int64_t modelId = requestPair.request.modelid();

    auto iter = mModels.find(modelId);
    if (iter == mModels.end()) {
      LOG(ERROR) << "Model ID " << modelId << " not prepared";
      RequestResponse response;
      response.set_error(GENERAL_FAILURE);
      CHECK_EQ(SendProto(mComm.get(), EXECUTE, response, /*payload=*/nullptr),
               0);
      mRequests.pop();
    }
    ModelPair& modelPair = iter->second;

    CHECK(modelPair.ready());

    // Allocate empty output pools.
    for (size_t i = 0; i < requestPair.pools.size(); i++) {
      if (requestPair.pools[i] == nullptr) {
        size_t size = requestPair.request.poolsizes(i);
        requestPair.pools[i] = easel::AllocateHardwareBuffer(size);
        CHECK(requestPair.pools[i]->Valid());
      }
    }

    std::vector<android::nn::RunTimePoolInfo> modelPoolInfos;
    setRunTimePoolInfosFromHardwareBuffer(&modelPoolInfos, &(modelPair.pools));

    std::vector<android::nn::RunTimePoolInfo> requestPoolInfos;
    setRunTimePoolInfosFromHardwareBuffer(&requestPoolInfos,
                                          &(requestPair.pools));

    android::nn::CpuExecutor executor;
    int n = executor.run(modelPair.model, requestPair.request, modelPoolInfos,
                         requestPoolInfos);
    LOG(INFO) << "executor.run returned " << n;
    ErrorStatus executionStatus =
        n == ANEURALNETWORKS_NO_ERROR ? NONE : GENERAL_FAILURE;

    if (executionStatus == ErrorStatus::NONE) {
      // Send output pools back to client.
      for (int outputIndex : requestPair.request.outputpools()) {
        requestPair.pools[outputIndex]->SetId(outputIndex);
        CHECK_EQ(mComm->Send(EXECUTE, requestPair.pools[outputIndex].get()), 0);
      }
    }

    RequestResponse response;
    response.set_error(executionStatus);
    CHECK_EQ(SendProto(mComm.get(), EXECUTE, response, /*payload=*/nullptr), 0);

    // Release allocated resource
    mRequests.pop();
  }
}

void EaselExecutorServer::handleDestroyModel(const easel::Message& message) {
  DestroyModelRequest request;
  CHECK(easel::MessageToProto(message, &request));
  int64_t modelId = request.modelid();
  bool success = false;
  {
    std::lock_guard<std::mutex> lock(mExecutorLock);
    auto iter = mModels.find(modelId);
    if (iter == mModels.end()) {
      success = false;
      LOG(ERROR) << "Destroyed model: model ID " << modelId << " not prepared";
    } else {
      mModels.erase(iter);
      success = true;
      LOG(INFO) << "Destroyed model: success, model ID " << modelId
                << ", models left " << mModels.size();
    }
  }

  DestroyModelResponse response;
  response.set_error(success ? NONE : INVALID_ARGUMENT);
  response.set_modelid(modelId);
  CHECK_EQ(easel::SendProto(mComm.get(), DESTROY_MODEL, response,
                            /*payload=*/nullptr),
           0);
}

}  // namespace paintbox_nn
