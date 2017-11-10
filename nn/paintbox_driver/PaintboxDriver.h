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

#ifndef PAINTBOX_NN_PAINTBOX_DRIVER_PAINTBOX_DRIVER_H
#define PAINTBOX_NN_PAINTBOX_DRIVER_PAINTBOX_DRIVER_H

#include "EaselExecutorClient.h"
#include "HalInterfaces.h"
#include "NeuralNetworks.h"

#include <string>

namespace android {
namespace nn {
namespace paintbox_driver {

// Paintbox NN API driver implementation.
class PaintboxDriver : public IDevice {
public:
    PaintboxDriver() : mName("paintbox") {}
    ~PaintboxDriver() override {}
    Return<ErrorStatus> prepareModel(const Model& model,
                                     const sp<IPreparedModelCallback>& callback) override;
    Return<DeviceStatus> getStatus() override;

    // Starts and runs the driver service.  Typically called from main().
    // This will return only once the service shuts down.
    int run();
protected:
    std::string mName;
    EaselExecutorClient mClient;
};

class PaintboxPreparedModel : public IPreparedModel {
public:
    PaintboxPreparedModel(const Model& model, EaselExecutorClient* client)
          : // Make a copy of the model, as we need to preserve it.
            mModel(model), mClient(client) {}
    ~PaintboxPreparedModel() override;
    bool initialize();
    Return<ErrorStatus> execute(const Request& request,
                                const sp<IExecutionCallback>& callback) override;
    const Model& model() const;

private:
    Model mModel;
    EaselExecutorClient* mClient;
};

} // namespace paintbox_driver
} // namespace nn
} // namespace android

#endif // PAINTBOX_NN_PAINTBOX_DRIVER_PAINTBOX_DRIVER_H
