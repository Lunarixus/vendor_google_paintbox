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

class PaintboxPreparedModel;

// Paintbox NN API driver implementation.
class PaintboxDriver : public IDevice {
 public:
  PaintboxDriver() : mName("paintbox"), mModelIdNext(1) {}
  ~PaintboxDriver() override {}
  Return<ErrorStatus> prepareModel(
      const Model& model, const sp<IPreparedModelCallback>& callback) override;
  Return<DeviceStatus> getStatus() override;

  // Starts and runs the driver service.  Typically called from main().
  // This will return only once the service shuts down.
  int run();

 protected:
  // Returns the next runtime model ID.
  // The returned value is monotonously increasing.
  int64_t nextModelId();

  std::string mName;
  EaselExecutorClient mClient;
  int64_t mModelIdNext;

  std::once_flag mInitialized;
};

class PaintboxPreparedModel : public IPreparedModel {
 public:
  PaintboxPreparedModel(const Model& model, int64_t modelId,
                        EaselExecutorClient* client);
  ~PaintboxPreparedModel() override;
  Return<ErrorStatus> execute(const Request& request,
                              const sp<IExecutionCallback>& callback) override;
  const Model* model();

 private:
  std::unique_ptr<Model> mModel;
  int64_t mModelId;
  EaselExecutorClient* mClient;
};

}  // namespace paintbox_driver
}  // namespace nn
}  // namespace android

#endif  // PAINTBOX_NN_PAINTBOX_DRIVER_PAINTBOX_DRIVER_H
