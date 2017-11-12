#ifndef IMX_SERVICE_H
#define IMX_SERVICE_H

#include <condition_variable>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <android-base/thread_annotations.h>

#include "EaselComm2.h"
#include "Imx.h"
#include "ImxChannels.h"
#include "vendor/google_paintbox/experimental/imx/libimxproto/include/imx.pb.h"

namespace imx {

class ImxService {
public:
    // No analysis when starting due to the use of mClient.
    ::ImxError start() NO_THREAD_SAFETY_ANALYSIS;

private:
    // Imx passthroughs. More live in the .cpp file, but the below calls
    // have a dependency on 'global' state.
    void createDeviceBufferSimple(
        const CreateDeviceBufferSimpleRequest& request);
    void lockDeviceBuffer(const LockDeviceBufferRequest& request);
    void unlockDeviceBuffer(
        const UnlockDeviceBufferRequest& request,
        const EaselComm2::Message& message);
    void executeFinishJob(const ExecuteFinishJobRequest& request);

    void registerHandlers();

    // A simple handler does not use DMA. The response will be autosent with
    // the returned status.
    template <typename RequestType, typename ResponseType>
    void registerSimpleHandler(
        ImxChannel channel,
        std::function<::ImxError(const RequestType&,
                                 ResponseType*)> func);

    // More complex handlers, ie those that do DMA, need to use the below
    // helper.
    template <typename RequestType>
    void registerHandler(
        ImxChannel channel,
        std::function<void(const RequestType&,
                           const EaselComm2::Message&)> func);

    // Sends a failure through a given channel. Expects Response to have a
    // set_status method.
    template <typename Response>
    void sendFailure(ImxChannel channel, Response* response)
        EXCLUDES(mClientLock);

    struct BufferRecord {
        BufferRecord(size_t in_size_bytes, void* in_vaddr=nullptr)
              : size_bytes(in_size_bytes), vaddr(in_vaddr) {}
        BufferRecord() : size_bytes(0), vaddr(nullptr) {}
        size_t size_bytes;
        void* vaddr;
    };

    std::mutex mBufferMapLock;
    std::mutex mClientLock ACQUIRED_AFTER(mBufferMapLock);
    std::unique_ptr<EaselComm2::Comm> mClient GUARDED_BY(mClientLock);
    std::unordered_map<ImxDeviceBufferHandle,
        BufferRecord> mBufferMap GUARDED_BY(mBufferMapLock);
};

}  // namespace imx

#endif // IMX_SERVICE_H
