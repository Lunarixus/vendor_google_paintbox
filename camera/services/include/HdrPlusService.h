#ifndef PAINTBOX_HDR_PLUS_SERVICE_H
#define PAINTBOX_HDR_PLUS_SERVICE_H

#include <mutex>

#include "easelcontrol.h"
#include "MessengerToHdrPlusClient.h"
#include "MessengerListenerFromHdrPlusClient.h"

namespace pbcamera {

class HdrPlusPipeline;

/**
 * HdrPlusService
 *
 * HdrPlusService class is a service that listens to messages from HdrPlusClient and performs
 * HDR+ processing.
 */
class HdrPlusService : public MessengerListenerFromHdrPlusClient {
public:
    HdrPlusService();
    virtual ~HdrPlusService();

    /*
     * Start service.
     * Returns:
     *  0:          if the service starts successfully.
     *  -EEXIST:    if the service is already started.
     *  -ENODEV:    if the service cannot be started due to a serious error.
     */
    status_t start();

    /*
     * Wait for the service to finish.
     * HDR+ service should be alive at all time, so this function will not return during normal
     * operations.
     */
    void wait();

private:
    // Callbacks from HDR+ client start here.
    // Override pbcamera::MessengerListenerFromHdrPlusClient
    status_t connect() override;
    void disconnect() override;
    void notifyClientClosed() override;
    status_t setStaticMetadata(const StaticMetadata& metadata);
    status_t configureStreams(const InputConfiguration &inputConfig,
            const std::vector<StreamConfiguration> &outputConfigs) override;
    status_t setZslHdrPlusMode(bool enabled) override;
    status_t submitCaptureRequest(const CaptureRequest &request) override;
    void notifyDmaInputBuffer(const DmaImageBuffer &dmaInputBuffer,
            int64_t mockingEaselTimestampNs) override;
    void notifyFrameMetadata(const FrameMetadata &metadata) override;
    // Callbacks from HDR+ client end here.

    // Stop the service with mApiLock held.
    void stopLocked();

    // Protect API methods from being called simultaneously.
    std::mutex mApiLock;

    std::condition_variable mExitCondition;

    // Easel control.
    EaselControlServer mEaselControl;

    // MessengerToHdrPlusClient to send messages to HDR+ client.
    std::shared_ptr<MessengerToHdrPlusClient> mMessengerToClient;

    // Pipeline of current use case.
    std::shared_ptr<HdrPlusPipeline> mPipeline;
};

} // namespace pbcamera

#endif // PAINTBOX_HDR_PLUS_SERVICE_H
