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
#include "ValidateHal.h"

#include <android-base/logging.h>
#include <hidl/LegacySupport.h>
#include <thread>

namespace android {
namespace nn {
namespace paintbox_driver {

Return<ErrorStatus> PaintboxDriver::prepareModel(const Model& model,
                                                 const sp<IPreparedModelCallback>& callback) {
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

    CHECK_EQ(mClient.initialize(), 0);

    {
        // Temporarily allow new model preparation overriding old one.
        // TODO(cjluo): This should be removed once we allow
        // full concurrent model preparation.
        sp<PaintboxPreparedModel> oldPreparedModel = mPreparedModel.promote();
        if (oldPreparedModel != nullptr) {
            LOG(WARNING) << "Old PaintboxPreparedModel still lives and will be overridden.";
            oldPreparedModel->destroyModel();
        }
    }

    {
        mPreparedModel = new PaintboxPreparedModel(model, &mClient);
        sp<PaintboxPreparedModel> strongPreparedModel = mPreparedModel.promote();

        // Use the model copy of the prepared model, because it is preserved.
        const Model* model = strongPreparedModel->model();
        CHECK(model != nullptr);
        mClient.prepareModel(*model, [callback, strongPreparedModel] (
                const paintbox_nn::PrepareModelResponse& response) {
            callback->notify(
                    paintbox_util::convertProtoError(response.error()), strongPreparedModel);
        });
    }

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

PaintboxPreparedModel::PaintboxPreparedModel(const Model& model, EaselExecutorClient* client)
      : mClient(client) {
    // Make a copy of the model, as we need to preserve it.
    mModel = std::make_unique<Model>();
    *mModel = model;
}

PaintboxPreparedModel::~PaintboxPreparedModel() {
    if (model() != nullptr) destroyModel();
}

const Model* PaintboxPreparedModel::model() {
    std::lock_guard<std::mutex> lock(mModelLock);
    return mModel.get();
}

void PaintboxPreparedModel::destroyModel() {
    std::lock_guard<std::mutex> lock(mModelLock);
    if (mModel == nullptr) return;
    mClient->destroyModel(*mModel);
    mModel = nullptr;
}

Return<ErrorStatus> PaintboxPreparedModel::execute(const Request& request,
                                                 const sp<IExecutionCallback>& callback) {
    std::lock_guard<std::mutex> lock(mModelLock);
    if (mModel == nullptr) {
        LOG(ERROR) << "Could not execute on an already destroyed model.";
        return ErrorStatus::GENERAL_FAILURE;
    }

    VLOG(DRIVER) << "execute(" << toString(request) << ")";
    if (callback.get() == nullptr) {
        LOG(ERROR) << "invalid callback passed to execute";
        return ErrorStatus::INVALID_ARGUMENT;
    }
    if (!validateRequest(request, *mModel)) {
        callback->notify(ErrorStatus::INVALID_ARGUMENT);
        return ErrorStatus::INVALID_ARGUMENT;
    }

    mClient->execute(request, [callback] (
            const paintbox_nn::RequestResponse& response) {
        ErrorStatus executionStatus = paintbox_util::convertProtoError(response.error());
        Return<void> returned = callback->notify(executionStatus);
        if (!returned.isOk()) {
            LOG(ERROR) << " hidl callback failed to return properly: " << returned.description();
        }
    });

    return ErrorStatus::NONE;
}

} // namespace paintbox_driver
} // namespace nn
} // namespace android
