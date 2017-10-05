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

    sp<PaintboxPreparedModel> preparedModel = new PaintboxPreparedModel(model, &mClient);
    mClient.prepareModel(model, [callback, preparedModel] (
            const paintbox_nn::PrepareModelResponse& response) {
        callback->notify(
                paintbox_util::convertProtoError(response.error()), preparedModel);
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

PaintboxPreparedModel::~PaintboxPreparedModel() {
    std::mutex destroyLock;
    std::condition_variable destroyComplete;
    mClient->destroyModel(mModel, [&](const paintbox_nn::TearDownModelResponse&){
        destroyComplete.notify_one();
    });

    std::unique_lock<std::mutex> lock;
    destroyComplete.wait(lock);
}

void PaintboxPreparedModel::asyncExecute(const Request& request,
                                       const sp<IExecutionCallback>& callback) {
    mClient->execute(request, [callback] (
            const paintbox_nn::RequestResponse& response) {
        ErrorStatus executionStatus = paintbox_util::convertProtoError(response.error());
        Return<void> returned = callback->notify(executionStatus);
        if (!returned.isOk()) {
            LOG(ERROR) << " hidl callback failed to return properly: " << returned.description();
        }
    });
}

Return<ErrorStatus> PaintboxPreparedModel::execute(const Request& request,
                                                 const sp<IExecutionCallback>& callback) {
    VLOG(DRIVER) << "execute(" << toString(request) << ")";
    if (callback.get() == nullptr) {
        LOG(ERROR) << "invalid callback passed to execute";
        return ErrorStatus::INVALID_ARGUMENT;
    }
    if (!validateRequest(request, mModel)) {
        callback->notify(ErrorStatus::INVALID_ARGUMENT);
        return ErrorStatus::INVALID_ARGUMENT;
    }

    asyncExecute(request, callback);

    return ErrorStatus::NONE;
}

} // namespace paintbox_driver
} // namespace nn
} // namespace android
