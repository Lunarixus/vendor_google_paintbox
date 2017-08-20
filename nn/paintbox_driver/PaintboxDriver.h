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

#include "CpuExecutor.h"
#include "EaselComm2.h"
#include "HalInterfaces.h"
#include "NeuralNetworks.h"

#include <condition_variable>
#include <mutex>

namespace android {
namespace nn {
namespace paintbox_driver {

// Paintbox NN API driver implementation.
class PaintboxDriver : public IDevice {
 public:
  virtual ~PaintboxDriver() {}
  virtual Return<void> initialize(initialize_cb _hidl_cb);
  virtual Return<void> getSupportedSubgraph(const Model& model,
                                            getSupportedSubgraph_cb _hidl_cb);
  virtual Return<sp<IPreparedModel>> prepareModel(const Model& model);
  virtual Return<DeviceStatus> getStatus();

 private:
  std::unique_ptr<EaselComm2::Comm> mComm;
};

class PaintboxPreparedModel : public IPreparedModel {
 public:
  PaintboxPreparedModel(const Model& model, EaselComm2::Comm* comm);
  virtual ~PaintboxPreparedModel() {}
  virtual Return<bool> execute(const Request& request);

 private:
  void executeCallback(const EaselComm2::Message& message);
  Model mModel;
  EaselComm2::Comm* mComm;
  std::mutex mExecuteMutex;
  std::condition_variable mExecuteDoneCond;
  bool mExecuteDone;
  int mExecuteReturn;
  RunTimePoolInfo* mOutput;
};

}  // namespace paintbox_driver
}  // namespace nn
}  // namespace android

#endif  // PAINTBOX_NN_PAINTBOX_DRIVER_PAINTBOX_DRIVER_H
