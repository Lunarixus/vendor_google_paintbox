/*
 * Copyright 2017 The Android Open Source Project
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

#ifndef IMX_CLIENT_H
#define IMX_CLIENT_H

#include <condition_variable>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <android-base/thread_annotations.h>

#include "EaselComm2.h"
#include "Imx.h"
#include "ImxChannels.h"
#include "ImxUtils.h"

namespace android {

class ImxClient {
public:
    ImxClient();
    static std::unique_ptr<ImxClient> create();
    ImxError getDefaultDevice(ImxDeviceHandle* handle);
    ImxCreateTransferNodeInfo defaultCreateTransferNodeInfo();
    ImxError createTransferNode(
        const ImxCreateTransferNodeInfo* info,
        ImxNodeHandle* node_handle_ptr);
    ImxError createGraph(
        const char* graph_name, const char* visa_string, ImxNodeHandle* nodes,
        const char** node_names, int node_count, ImxGraphHandle* graph_handle);
    ImxError compileGraph(
        ImxGraphHandle graph, const ImxCompileGraphInfo* info,
        ImxCompiledGraphHandle* compiled_handle);
    ImxError deleteGraph(ImxGraphHandle graph);
    ImxError createJob(
        ImxCompiledGraphHandle graph_handle, ImxJobHandle* job_handle_ptr);
    ImxError createDeviceBufferSimple(
        uint64_t size_bytes, int flags,
        ImxDeviceBufferHandle* buffer_handle_ptr);
    ImxError deleteDevice(ImxDeviceHandle device_handle);
    ImxError finalizeBuffers(
        ImxJobHandle job_handle, const ImxFinalizeBufferInfo* info,
        int num_info);
    ImxError lockDeviceBuffer(
        ImxDeviceBufferHandle buffer_handle, void** vaddr);
    ImxError unlockDeviceBuffer(ImxDeviceBufferHandle buffer_handle);
    ImxError executeJob(ImxJobHandle job_handle);

private:
    ImxError start();

    // Helpers to allow synchronus message passing
    void wait();
    void signal();

    // Takes a Response proto message and a callback.
    // The Response proto must have a status field.
    // The callback will only be called if the response is successful.
    // Currently this will wait forever.
    template <typename Response>
    ImxError sendAndWait(
        imx::ImxChannel channel,
        const ::google::protobuf::MessageLite& request,
        std::function<void(const Response&)> func = [](const Response&){});

    std::mutex mBufferMapLock;
    std::mutex mClientLock ACQUIRED_AFTER(mBufferMapLock);
    std::mutex mReceiveLock ACQUIRED_AFTER(mBufferMapLock);
    std::unique_ptr<EaselComm2::Comm> mClient GUARDED_BY(mClientLock);
    std::condition_variable mReceiveCond;
    std::unordered_map<ImxDeviceBufferHandle,
        imx::BufferRecord> mBufferMap GUARDED_BY(mBufferMapLock);

    // mReceived is used to track whether or not we have recieved a response
    // from the ImxService.
    // NOTE: This can go as low as -1 in the event the ImxService response
    // to a request before we start waiting for it.
    // We'd like to guard this too, but unfortunately its use in a lambda seems
    // to be too sophisticated for the analysis used here.
    int mReceived;  // GUARDED_BY(mReceiveLock)
};

}  // namespace android

#endif // IMX_CLIENT_H
