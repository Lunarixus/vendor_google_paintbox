#include "Imx.h"

#include <memory>
#include <mutex>

#include "ImxClient.h"

std::unique_ptr<android::ImxClient> gImxClient = android::ImxClient::create();

ImxError ImxGetDefaultDevice(ImxDeviceHandle* device_handle) {
    return gImxClient->getDefaultDevice(device_handle);
}

ImxCreateTransferNodeInfo ImxDefaultCreateTransferNodeInfo() {
    return gImxClient->defaultCreateTransferNodeInfo();
}

ImxError ImxCreateTransferNode(
    const ImxCreateTransferNodeInfo *info, ImxNodeHandle *node_handle_ptr) {
    return gImxClient->createTransferNode(info, node_handle_ptr);
}

ImxError ImxCreateGraph(
    const char *graph_name, const char *visa_string, ImxNodeHandle *nodes,
    const char **node_names, int node_count,
    ImxGraphHandle* graph_handle_ptr) {
    return gImxClient->createGraph(
        graph_name, visa_string, nodes, node_names, node_count, graph_handle_ptr);
}

ImxError ImxCompileGraph(
    ImxGraphHandle graph, const ImxCompileGraphInfo* info,
    ImxCompiledGraphHandle* compiled_handle) {
    return gImxClient->compileGraph(graph, info, compiled_handle);
}

ImxError ImxCreateJob(
    ImxCompiledGraphHandle compiled_graph, ImxJobHandle* job_handle_ptr) {
    return gImxClient->createJob(compiled_graph, job_handle_ptr);
}

ImxError ImxCreateDeviceBufferSimple(
    uint64_t size_bytes, int flags, ImxDeviceBufferHandle* buffer_handle_ptr) {
    return gImxClient->createDeviceBufferSimple(size_bytes, flags, buffer_handle_ptr);
}

ImxError ImxDeleteGraph(ImxGraphHandle graph_handle) {
    return gImxClient->deleteGraph(graph_handle);
}

ImxError ImxDeleteDevice(ImxDeviceHandle device_handle) {
    return gImxClient->deleteDevice(device_handle);
}

ImxError ImxFinalizeBuffers(
    ImxJobHandle job_handle, const ImxFinalizeBufferInfo* info, int num_info) {
    return gImxClient->finalizeBuffers(job_handle, info, num_info);
}

ImxError ImxLockDeviceBuffer(ImxDeviceBufferHandle buffer_handle, void** vaddr) {
    return gImxClient->lockDeviceBuffer(buffer_handle, vaddr);
}

ImxError ImxUnlockDeviceBuffer(ImxDeviceBufferHandle buffer_handle) {
    return gImxClient->unlockDeviceBuffer(buffer_handle);
}

ImxError ImxExecuteJob(ImxJobHandle job_handle) {
    return gImxClient->executeJob(job_handle);
}
