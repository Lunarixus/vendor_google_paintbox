#define LOG_TAG "ImxService"
#include "ImxService.h"

#include <log/log.h>

#include "ImxProtoConversions.h"

namespace imx {

// Below are Imx Passthroughs to libimageprocessor. These are standalone
// functions becuase they do not depend on any service state.
::ImxError getDefaultDevice(
    const GetDefaultDeviceRequest&, GetDefaultDeviceResponse* response) {
    ImxDeviceHandle device_handle;
    ::ImxError ret = ImxGetDefaultDevice(&device_handle);
    response->set_device_handle(reinterpret_cast<int64_t>(device_handle));
    return ret;
}

::ImxError defaultCreateTransferNodeInfo(
    const DefaultCreateTransferNodeInfoRequest&,
    DefaultCreateTransferNodeInfoResponse* response) {
    ConvertCreateTransferNodeInfo(
        ImxDefaultCreateTransferNodeInfo(),
        response->mutable_create_transfer_node_info());
    return ::IMX_SUCCESS;
}

::ImxError createTransferNode(
    const CreateTransferNodeRequest& request,
    CreateTransferNodeResponse* response) {
    ImxNodeHandle node_handle;
    ImxCreateTransferNodeInfo node_info;
    ConvertCreateTransferNodeInfo(request.create_transfer_node_info(), &node_info);
    ::ImxError ret = ImxCreateTransferNode(&node_info, &node_handle);
    response->set_node_handle(reinterpret_cast<int64_t>(node_handle));
    return ret;
}

::ImxError compileGraph(
    const CompileGraphRequest& request, CompileGraphResponse* response) {
    // Rebuild struct
    const auto& info_proto = request.info();
    int num_params = info_proto.params().size();
    std::vector<ImxParameterSetting> params(num_params);
    for (int i = 0; i < num_params; i++) {
        ConvertParameterSetting(info_proto.params(i), &params[i]);
    }

    int num_options = info_proto.options().size();
    std::vector<ImxCompileGraphOptionSetting> options(num_options);
    for (int i = 0; i < num_options; i++) {
        ConvertCompileGraphOptionSetting(info_proto.options(i), &options[i]);
    }

    // Setup ImxCompileGraphInfo
    ImxCompileGraphInfo info;
    info.device = (ImxDeviceHandle)info_proto.device();
    info.params = num_params ? params.data() : nullptr;
    info.num_params = num_params;
    info.options = num_options ? options.data() : nullptr;
    info.num_options = num_options;

    ImxCompiledGraphHandle compiled_handle;
    ::ImxError ret =
        ImxCompileGraph(
            reinterpret_cast<ImxGraphHandle>(request.graph()),
            &info, &compiled_handle);
    response->set_compiled_handle(reinterpret_cast<int64_t>(compiled_handle));
    return ret;
}

::ImxError deleteGraph(
    const DeleteGraphRequest& request, DeleteGraphResponse*) {
    ImxGraphHandle graph_handle =
        reinterpret_cast<ImxGraphHandle>(request.graph_handle());
    return ImxDeleteGraph(graph_handle);
}

::ImxError createJob(
    const CreateJobRequest& request, CreateJobResponse* response) {
    ImxCompiledGraphHandle compiled_graph_handle =
        reinterpret_cast<ImxCompiledGraphHandle>(request.compiled_graph_handle());
    ImxJobHandle job_handle;
    ::ImxError ret = ImxCreateJob(compiled_graph_handle, &job_handle);
    response->set_job_handle(reinterpret_cast<int64_t>(job_handle));
    return ret;
}

::ImxError deleteDevice(
    const DeleteDeviceRequest& request, DeleteDeviceResponse*) {
    ImxDeviceHandle device_handle =
        reinterpret_cast<ImxDeviceHandle>(request.device_handle());
    return ImxDeleteDevice(device_handle);
}

::ImxError finalizeBuffers(
    const FinalizeBuffersRequest& request, FinalizeBuffersResponse*) {
    // Rebuild struct
    int num_infos = request.infos().size();
    std::vector<ImxFinalizeBufferInfo> infos(num_infos);
    for (int i = 0; i < num_infos; i++) {
        ConvertFinalizeBufferInfo(request.infos(i), &infos[i]);
    }

    return
        ImxFinalizeBuffers(
            reinterpret_cast<ImxJobHandle>(request.job_handle()),
            num_infos ? infos.data() : nullptr,
            num_infos);
}

::ImxError createGraph(
    const CreateGraphRequest& request, CreateGraphResponse* response) {
    // Rebuild raw nodes
    int node_count = request.nodes().size();
    std::vector<ImxNodeHandle> nodes(node_count);
    std::vector<const char*> node_names(node_count);
    for (int i = 0; i < node_count; i++) {
        nodes[i] = (ImxNodeHandle)request.nodes(i).handle();
        node_names[i] = request.nodes(i).name().c_str();
    }
    ImxGraphHandle graph_handle;
    ::ImxError ret =
        ImxCreateGraph(
            request.has_graph_name() ? request.graph_name().c_str() : nullptr,
            request.visa_string().c_str(),
            node_count ? nodes.data() : nullptr,
            node_count ? node_names.data() : nullptr,
            node_count, &graph_handle);
    response->set_graph_handle(reinterpret_cast<int64_t>(graph_handle));
    return ret;
}

::ImxError executeJob(
    const ExecuteJobRequest& request, ExecuteJobResponse*) {
    ImxJobHandle job_handle =
        reinterpret_cast<ImxJobHandle>(request.job_handle());
    return ImxExecuteJob(job_handle);
}

::ImxError ImxService::start() {
    mClient = EaselComm2::Comm::create(EaselComm2::Comm::Mode::SERVER);
    if (!mClient) {
        ALOGE("%s: Unable to create EaselComm.", __FUNCTION__);
        return ::IMX_FAILURE;
    }

    registerHandlers();
    int ret = mClient->openPersistent(EASEL_SERVICE_IMX);
    if (ret != 0) {
        ALOGE("%s: Unable to open ImxService.", __FUNCTION__);
        return ::IMX_FAILURE;
    }
    return ::IMX_SUCCESS;
}

template <typename RequestType, typename ResponseType>
void ImxService::registerSimpleHandler(
    ImxChannel channel,
    std::function<::ImxError(const RequestType&,
                             ResponseType*)> func) {
    std::lock_guard<std::mutex> lock(mClientLock);
    mClient->registerHandler(
        channel,
        [func, channel, this](const EaselComm2::Message& message) {
           std::lock_guard<std::mutex> lock(mClientLock);
           RequestType request;
           message.toProto(&request);
           ResponseType response;
           ::ImxError ret = func(request, &response);
           response.set_status(ConvertStatus(ret));
           mClient->send(channel, response);
        });
}

template <typename RequestType>
void ImxService::registerHandler(
    ImxChannel channel,
    std::function<void(const RequestType&,
                       const EaselComm2::Message&)> func) {
    std::lock_guard<std::mutex> lock(mClientLock);
    mClient->registerHandler(
        channel,
        [func](const EaselComm2::Message& message) {
           RequestType request;
           message.toProto(&request);
           func(request, message);
        });
}

void ImxService::registerHandlers() {
    registerSimpleHandler<GetDefaultDeviceRequest, GetDefaultDeviceResponse>(
        kGetDefaultDeviceChannel, getDefaultDevice);
    registerSimpleHandler<
        DefaultCreateTransferNodeInfoRequest,
        DefaultCreateTransferNodeInfoResponse>(
        kDefaultCreateTransferNodeInfoChannel,
        defaultCreateTransferNodeInfo);
    registerSimpleHandler<CreateTransferNodeRequest, CreateTransferNodeResponse>(
        kCreateTransferNodeChannel, createTransferNode);
    registerSimpleHandler<CreateGraphRequest, CreateGraphResponse>(
        kCreateGraphChannel, createGraph);
    registerSimpleHandler<CompileGraphRequest, CompileGraphResponse>(
        kCompileGraphChannel, compileGraph);
    registerSimpleHandler<DeleteGraphRequest, DeleteGraphResponse>(
        kDeleteGraphChannel, deleteGraph);
    registerSimpleHandler<CreateJobRequest, CreateJobResponse>(
        kCreateJobChannel, createJob);
    registerSimpleHandler<DeleteDeviceRequest, DeleteDeviceResponse>(
        kDeleteDeviceChannel, deleteDevice);
    registerSimpleHandler<FinalizeBuffersRequest, FinalizeBuffersResponse>(
        kFinalizeBuffersChannel, finalizeBuffers);
    registerSimpleHandler<ExecuteJobRequest, ExecuteJobResponse>(
        kExecuteJobChannel, executeJob);

    // Buffer management requires more delicate controls.
    registerHandler<CreateDeviceBufferSimpleRequest>(
        kCreateDeviceBufferSimpleChannel,
        [this](const CreateDeviceBufferSimpleRequest& request,
               const EaselComm2::Message&) {
          createDeviceBufferSimple(request); });
    registerHandler<LockDeviceBufferRequest>(
        kLockDeviceBufferChannel,
        [this](const LockDeviceBufferRequest& r,
               const EaselComm2::Message&) { lockDeviceBuffer(r); });
    registerHandler<UnlockDeviceBufferRequest>(
        kUnlockDeviceBufferChannel,
        [this](const UnlockDeviceBufferRequest& r,
               const EaselComm2::Message& m) { unlockDeviceBuffer(r, m); });
}

void ImxService::createDeviceBufferSimple(
    const CreateDeviceBufferSimpleRequest& request) {
    std::lock_guard<std::mutex> lock(mBufferMapLock);
    CreateDeviceBufferSimpleResponse response;
    ImxDeviceBufferHandle buffer_handle;
    response.set_status(
        ConvertStatus(
            ImxCreateDeviceBufferSimple(
                request.size_bytes(), request.flags(), &buffer_handle)));
    response.set_buffer_handle(
        reinterpret_cast<int64_t>(buffer_handle));
    mBufferMap[buffer_handle] = BufferRecord(request.size_bytes());
    {
        std::lock_guard<std::mutex> lock(mClientLock);
        mClient->send(kCreateDeviceBufferSimpleChannel, response);
    }
}

template <typename Response>
void ImxService::sendFailure(ImxChannel channel, Response* response) {
    std::lock_guard<std::mutex> lock(mClientLock);
    response->set_status(::imx::IMX_FAILURE);
    mClient->send(channel, *response);
}

void ImxService::lockDeviceBuffer(const LockDeviceBufferRequest& request) {
    std::lock_guard<std::mutex> lock(mBufferMapLock);
    ImxDeviceBufferHandle buffer_handle =
        reinterpret_cast<ImxDeviceBufferHandle>(request.buffer_handle());

    // Lookup buffer handle and stash our new vaddr.
    LockDeviceBufferResponse response;
    auto buffer_record_it = mBufferMap.find(buffer_handle);
    if (buffer_record_it == mBufferMap.end()) {
        ALOGE("%s: easel invalid buffer.", __FUNCTION__);
        sendFailure(kLockDeviceBufferChannel, &response);
        return;
    }
    auto& buffer_record = buffer_record_it->second;

    void* vaddr;
    response.set_status(
        ConvertStatus(
            ImxLockDeviceBuffer(buffer_handle, &vaddr)));

    buffer_record.vaddr = vaddr;

    // Attach HardwareBuffer to message
    int fd;
    if (ImxShareDeviceBuffer(buffer_handle, &fd) != IMX_SUCCESS) {
        ALOGE("%s: easel cannot share.", __FUNCTION__);
        sendFailure(kLockDeviceBufferChannel, &response);
        return;
    }
    EaselComm2::HardwareBuffer hardware_buffer(
        fd, buffer_record.size_bytes);
    {
        std::lock_guard<std::mutex> lock(mClientLock);
        mClient->send(kLockDeviceBufferChannel, response, &hardware_buffer);
    }
}

void ImxService::unlockDeviceBuffer(
    const UnlockDeviceBufferRequest& request,
    const EaselComm2::Message& message) {
    std::lock_guard<std::mutex> lock(mBufferMapLock);
    UnlockDeviceBufferResponse response;
    ImxDeviceBufferHandle buffer_handle =
        (ImxDeviceBufferHandle)request.buffer_handle();

    // Lookup buffer handle and copy payload
    auto buffer_record_it = mBufferMap.find(buffer_handle);
    if (buffer_record_it == mBufferMap.end()) {
        ALOGE("%s: easel invalid buffer.", __FUNCTION__);
        sendFailure(kUnlockDeviceBufferChannel, &response);
        return;
    }

    auto& buffer_record = buffer_record_it->second;

    // Attach HardwareBuffer to message
    int fd;
    if (ImxShareDeviceBuffer(buffer_handle, &fd) != IMX_SUCCESS) {
        ALOGE("%s: easel cannot share.", __FUNCTION__);
        sendFailure(kUnlockDeviceBufferChannel, &response);
        return;
    }

    int ret;
    {
        std::lock_guard<std::mutex> lock(mClientLock);
        EaselComm2::HardwareBuffer hardware_buffer(
            fd, buffer_record.size_bytes);
        ret = mClient->receivePayload(message, &hardware_buffer);
    }
    if (ret) {
        ALOGE("%s: easel could not receive payload", __FUNCTION__);
        sendFailure(kUnlockDeviceBufferChannel, &response);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mClientLock);
        response.set_status(ConvertStatus(ImxUnlockDeviceBuffer(buffer_handle)));
        mClient->send(kUnlockDeviceBufferChannel, response);
    }
}

}  // namespace imx
