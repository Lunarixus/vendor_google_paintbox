/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "PaintboxDriver"

#include "PaintboxDriver.h"

#include "Conversion.h"
#include "CpuExecutor.h"
#include "HalInterfaces.h"

#include <android-base/logging.h>
#include <hidl/LegacySupport.h>
#include <thread>

namespace android {
namespace nn {
namespace paintbox_driver {

int64_t PaintboxDriver::nextModelId() { return mModelIdNext++; }

Return<ErrorStatus> PaintboxDriver::prepareModel(
    const Model& model, const sp<IPreparedModelCallback>& callback) {
  if (VLOG_IS_ON(DRIVER)) {
    VLOG(DRIVER) << "prepareModel";
    logModelToInfo(model);
  }
  if (callback.get() == nullptr) {
    LOG(ERROR) << "invalid callback passed to prepareModel";
    return ErrorStatus::INVALID_ARGUMENT;
  }
  if (!validateModel(model)) {
    callback->notify(ErrorStatus::INVALID_ARGUMENT, nullptr);
    return ErrorStatus::INVALID_ARGUMENT;
  }

  std::call_once(mInitialized, [&] { CHECK_EQ(mClient.initialize(), 0); });

  int64_t modelId = nextModelId();
  sp<PaintboxPreparedModel> strongPreparedModel(
      new PaintboxPreparedModel(model, modelId, &mClient));

  // Use the model copy of the prepared model, because it is preserved.
  const Model* modelPointer = strongPreparedModel->model();
  mClient.prepareModel(
      *modelPointer, modelId,
      [callback,
       strongPreparedModel](const paintbox_nn::PrepareModelResponse& response) {
        callback->notify(paintbox_util::convertProtoError(response.error()),
                         strongPreparedModel);
      });

  return ErrorStatus::NONE;
}

Return<DeviceStatus> PaintboxDriver::getStatus() {
  VLOG(DRIVER) << "getStatus()";
  return DeviceStatus::AVAILABLE;
}

int PaintboxDriver::run() {
  android::hardware::configureRpcThreadpool(1, true);
  if (registerAsService(mName) != android::OK) {
    LOG(ERROR) << "Could not register service";
    return 1;
  }
  android::hardware::joinRpcThreadpool();
  LOG(ERROR) << "Service exited!";
  return 1;
}

PaintboxPreparedModel::PaintboxPreparedModel(const Model& model,
                                             int64_t modelId,
                                             EaselExecutorClient* client)
    : mModelId(modelId), mClient(client) {
  // Make a copy of the model, as we need to preserve it.
  mModel = std::make_unique<Model>();
  *mModel = model;
}

PaintboxPreparedModel::~PaintboxPreparedModel() {
  mClient->destroyModel(mModelId);
}

const Model* PaintboxPreparedModel::model() { return mModel.get(); }

Return<ErrorStatus> PaintboxPreparedModel::execute(
    const Request& request, const sp<IExecutionCallback>& callback) {
  VLOG(DRIVER) << "execute(" << toString(request) << ")";
  if (callback.get() == nullptr) {
    LOG(ERROR) << "invalid callback passed to execute";
    return ErrorStatus::INVALID_ARGUMENT;
  }
  if (!validateRequest(request, *mModel)) {
    callback->notify(ErrorStatus::INVALID_ARGUMENT);
    return ErrorStatus::INVALID_ARGUMENT;
  }

  mClient->execute(request, mModelId,
                   [callback](const paintbox_nn::RequestResponse& response) {
                     ErrorStatus executionStatus =
                         paintbox_util::convertProtoError(response.error());
                     Return<void> returned = callback->notify(executionStatus);
                     if (!returned.isOk()) {
                       LOG(ERROR)
                           << " hidl callback failed to return properly: "
                           << returned.description();
                     }
                   });

  return ErrorStatus::NONE;
}

}  // namespace paintbox_driver
}  // namespace nn
}  // namespace android
