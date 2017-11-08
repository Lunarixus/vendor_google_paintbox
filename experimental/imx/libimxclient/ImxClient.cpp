#define LOG_TAG "ImxClient"
#include <log/log.h>

#include "ImxClient.h"

#include "android-base/logging.h"
#include "ImxProtoConversions.h"
#include "vendor/google_paintbox/experimental/imx/libimxproto/include/imx.pb.h"

namespace android {

std::unique_ptr<ImxClient> ImxClient::create() {
    auto client_impl = std::make_unique<ImxClient>();
    if (client_impl->start() != IMX_SUCCESS) {
        return nullptr;
    }
    return client_impl;
}

ImxClient::ImxClient() : mReceived(0) {
}

template <typename Response>
::ImxError ImxClient::sendAndWait(
    imx::ImxChannel channel,
    const ::google::protobuf::MessageLite& request,
    std::function<void(const Response&)> func) {
    std::lock_guard<std::mutex> lock(mClientLock);
    Response response;
    mClient->registerHandler(
        channel,
        [&](const EaselComm2::Message& m) {
            CHECK(m.toProto(&response));
            signal();
        });
    int ret = mClient->send(channel, request);
    if (ret != 0) {
      return ::IMX_FAILURE;
    }
    wait();
    if (response.status() == ::IMX_SUCCESS) {
        func(response);
    }
    return imx::ConvertStatus(response.status());
}

ImxError ImxClient::getDefaultDevice(ImxDeviceHandle* device_handle) {
    imx::GetDefaultDeviceRequest request;
    return sendAndWait<imx::GetDefaultDeviceResponse>(
        imx::kGetDefaultDeviceChannel, request,
        [&] (const imx::GetDefaultDeviceResponse& response) {
            *device_handle = (ImxDeviceHandle)response.device_handle();
        });
}

ImxCreateTransferNodeInfo ImxClient::defaultCreateTransferNodeInfo() {
    imx::DefaultCreateTransferNodeInfoRequest request;
    ImxCreateTransferNodeInfo out;
    sendAndWait<imx::DefaultCreateTransferNodeInfoResponse>(
        imx::kDefaultCreateTransferNodeInfoChannel, request,
        [&] (const imx::DefaultCreateTransferNodeInfoResponse& response) {
            imx::ConvertCreateTransferNodeInfo(
                response.create_transfer_node_info(), &out);
        });
    return out;
}

ImxError ImxClient::createTransferNode(
    const ImxCreateTransferNodeInfo* info,
    ImxNodeHandle* node_handle_ptr) {
    imx::CreateTransferNodeRequest request;
    ConvertCreateTransferNodeInfo(
        *info, request.mutable_create_transfer_node_info());
    return sendAndWait<imx::CreateTransferNodeResponse>(
        imx::kCreateTransferNodeChannel, request,
        [&] (const imx::CreateTransferNodeResponse& response) {
            *node_handle_ptr = (ImxNodeHandle)response.node_handle();
        });
}

ImxError ImxClient::createGraph(
    const char* graph_name, const char* visa_string, ImxNodeHandle* nodes,
    const char** node_names, int node_count, ImxGraphHandle* graph_handle_ptr) {
    // Fill out request
    imx::CreateGraphRequest request;
    if (graph_name) {
        request.set_graph_name(graph_name);
    }
    request.set_visa_string(visa_string);
    for (int i = 0; i < node_count; i++) {
        auto* node = request.add_nodes();
        node->set_handle((int64_t)nodes[i]);
        node->set_name(node_names[i]);
    }
    return sendAndWait<imx::CreateGraphResponse>(
        imx::kCreateGraphChannel, request,
        [&] (const imx::CreateGraphResponse& response) {
            *graph_handle_ptr = (ImxGraphHandle)response.graph_handle();
        });
}

ImxError ImxClient::compileGraph(
    ImxGraphHandle graph, const ImxCompileGraphInfo* info,
    ImxCompiledGraphHandle* compiled_handle) {
    imx::CompileGraphRequest request;
    request.set_graph((int64_t)graph);
    ConvertCompileGraphInfo(*info, request.mutable_info());
    return sendAndWait<imx::CompileGraphResponse>(
        imx::kCompileGraphChannel, request,
        [&] (const imx::CompileGraphResponse& response) {
            *compiled_handle = (ImxCompiledGraphHandle)response.compiled_handle();
        });
}

ImxError ImxClient::deleteGraph(ImxGraphHandle graph_handle) {
    imx::DeleteGraphRequest request;
    request.set_graph_handle((int64_t)graph_handle);
    return sendAndWait<imx::DeleteGraphResponse>(
        imx::kDeleteGraphChannel, request);
}

ImxError ImxClient::createJob(
    ImxCompiledGraphHandle compiled_graph_handle, ImxJobHandle* job_handle_ptr) {
    imx::CreateJobRequest request;
    request.set_compiled_graph_handle((int64_t)compiled_graph_handle);
    return sendAndWait<imx::CreateJobResponse>(
        imx::kCreateJobChannel, request,
        [&] (const imx::CreateJobResponse& response) {
            *job_handle_ptr = (ImxJobHandle)response.job_handle();
        });
}

ImxError ImxClient::createDeviceBufferSimple(
    uint64_t size_bytes, int flags, ImxDeviceBufferHandle* buffer_handle_ptr) {
    imx::CreateDeviceBufferSimpleRequest request;
    request.set_size_bytes(size_bytes);
    request.set_flags(flags);
    return sendAndWait<imx::CreateDeviceBufferSimpleResponse>(
        imx::kCreateDeviceBufferSimpleChannel, request,
        [&] (const imx::CreateDeviceBufferSimpleResponse& response) {
            std::lock_guard<std::mutex> lock(mBufferMapLock);
            ImxDeviceBufferHandle buffer_handle =
                (ImxDeviceBufferHandle)response.buffer_handle();
            *buffer_handle_ptr = buffer_handle;
            // Stash the handle in our map.
            mBufferMap[buffer_handle] = imx::BufferRecord(size_bytes);
        });
}

ImxError ImxClient::deleteDevice(ImxDeviceHandle device_handle) {
    imx::DeleteDeviceRequest request;
    request.set_device_handle((int64_t)device_handle);
    return sendAndWait<imx::DeleteDeviceResponse>(
        imx::kDeleteDeviceChannel, request);
}

ImxError ImxClient::finalizeBuffers(
    ImxJobHandle job_handle, const ImxFinalizeBufferInfo* info, int num_info) {
    imx::FinalizeBuffersRequest request;
    request.set_job_handle((int64_t)job_handle);
    for (int i = 0; i < num_info; i++) {
        ConvertFinalizeBufferInfo(info[i], request.add_infos());
    }
    return sendAndWait<imx::FinalizeBuffersResponse>(
        imx::kFinalizeBuffersChannel, request);
}

ImxError ImxClient::lockDeviceBuffer(
    ImxDeviceBufferHandle buffer_handle, void** vaddr) {
    std::lock_guard<std::mutex> lock(mBufferMapLock);

    // Lookup buffer handle
    auto buffer_record_it = mBufferMap.find(buffer_handle);
    if (buffer_record_it == mBufferMap.end()) {
        ALOGE("%s: easel invalid buffer.", __FUNCTION__);
        return ::IMX_FAILURE;
    }
    auto& buffer_record = buffer_record_it->second;

    imx::LockDeviceBufferRequest request;
    request.set_buffer_handle((int64_t)buffer_handle);
    imx::LockDeviceBufferResponse response;
    int ret = ::IMX_SUCCESS;
    {
        std::lock_guard<std::mutex> lock(mClientLock);
        mClient->registerHandler(
            imx::kLockDeviceBufferChannel,
            [&](const EaselComm2::Message& m) {
                std::lock_guard<std::mutex> lock(mClientLock);
                CHECK(m.toProto(&response));
                CHECK(m.hasPayload());
                EaselComm2::HardwareBuffer hardware_buffer(
                    buffer_record.vaddr(), buffer_record.size_bytes());
                CHECK_EQ(mClient->receivePayload(m, &hardware_buffer), 0);
                signal();
            });

        ret = mClient->send(imx::kLockDeviceBufferChannel, request);
    }
    if (ret != 0) {
        ALOGE("%s: easel failed.", __FUNCTION__);
        return IMX_FAILURE;
    }
    wait();
    *vaddr = buffer_record.vaddr();
    return imx::ConvertStatus(response.status());
}

ImxError ImxClient::unlockDeviceBuffer(ImxDeviceBufferHandle buffer_handle) {
    std::lock_guard<std::mutex> lock(mBufferMapLock);
    // Lookup buffer handle
    auto buffer_record_it = mBufferMap.find(buffer_handle);
    if (buffer_record_it == mBufferMap.end()) {
        ALOGE("%s: easel invalid buffer.", __FUNCTION__);
        return ::IMX_FAILURE;
    }
    auto& buffer_record = buffer_record_it->second;
    imx::UnlockDeviceBufferRequest request;
    request.set_buffer_handle((int64_t)buffer_handle);
    imx::UnlockDeviceBufferResponse response;
    int ret = ::IMX_SUCCESS;
    {
        std::lock_guard<std::mutex> lock(mClientLock);
        mClient->registerHandler(
            imx::kUnlockDeviceBufferChannel,
            [&](const EaselComm2::Message& m) {
                CHECK(m.toProto(&response));
                signal();
            });

        // Setup HardwareBuffer
        EaselComm2::HardwareBuffer hardware_buffer(
            buffer_record.vaddr(), buffer_record.size_bytes());
        ret = mClient->send(
            imx::kUnlockDeviceBufferChannel, request, &hardware_buffer);
    }
    if (ret != 0) {
        ALOGE("%s: easel failed.", __FUNCTION__);
        return ::IMX_FAILURE;
    }
    wait();
    return imx::ConvertStatus(response.status());
}

ImxError ImxClient::executeJob(ImxJobHandle job_handle) {
    imx::ExecuteJobRequest request;
    request.set_job_handle((int64_t)job_handle);
    return sendAndWait<imx::ExecuteJobResponse>(
        imx::kExecuteJobChannel, request);
}

ImxError ImxClient::start() {
    std::lock_guard<std::mutex> lock(mClientLock);
    mClient = EaselComm2::Comm::create(EaselComm2::Comm::Mode::CLIENT);
    if (!mClient) {
        ALOGE("%s: Unable to create EaselComm2 Client.", __FUNCTION__);
        return ::IMX_FAILURE;
    }

    int ret = mClient->open(EASEL_SERVICE_IMX);
    if (ret != 0) {
        ALOGE("%s: Unable to open Imx service.", __FUNCTION__);
        return ::IMX_FAILURE;
    }

    ret = mClient->startReceiving();
    if (ret != 0) {
        ALOGE("%s: Unable to open Imx service.", __FUNCTION__);
        return ::IMX_FAILURE;
    }
    return ::IMX_SUCCESS;
}

void ImxClient::wait() {
    std::unique_lock<std::mutex> lock(mReceiveLock);
    CHECK(mReceived >= -1);
    mReceived++;
    mReceiveCond.wait(lock, [&] { return mReceived == 0; });
}

void ImxClient::signal() {
    {
        std::lock_guard<std::mutex> lock(mReceiveLock);
        mReceived--;
    }
    mReceiveCond.notify_one();
}

}  // namespace android
