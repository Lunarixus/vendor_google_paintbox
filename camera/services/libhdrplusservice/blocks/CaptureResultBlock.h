#ifndef PAINTBOX_HDR_PLUS_PIPELINE_CAPTURE_RESULT_BLOCK_H
#define PAINTBOX_HDR_PLUS_PIPELINE_CAPTURE_RESULT_BLOCK_H

#include "PipelineBuffer.h"
#include "MessengerToHdrPlusClient.h"
#include "PipelineBlock.h"

namespace pbcamera {

/*
 * CaptureResultBlock is a pipeline block that sends CaptureResult compiled with
 * PipelineBlock::Input to the client using an MessengerToHdrPlusClient. CaptureResultBlock's
 * doWorkLocked() starts its work when a PipelineBlock::Input is available.
 * PipelineBlock::OutputRequest is ignored in CaptureResultBlock.
 */
class CaptureResultBlock : public PipelineBlock {
public:
    virtual ~CaptureResultBlock();

    /*
     * Create a CaptureResultBlock.
     *
     * pipeline is the pipeline this block belongs to.
     * messenger is an EaselMessenger for sending CaptureResult to the client.
     *
     * Returns a std::shared_ptr<CaptureResultBlock> pointing to a CaptureResultBlock on success.
     * Returns a std::shared_ptr<CaptureResultBlock> pointing to nullptr if it failed.
     */
    static std::shared_ptr<CaptureResultBlock> newCaptureResultBlock(
            std::weak_ptr<HdrPlusPipeline> pipeline,
            std::shared_ptr<MessengerToHdrPlusClient> messenger);

    bool doWorkLocked() override;
private:
    // Use newCaptureResultBlock to create a CaptureResultBlock.
    CaptureResultBlock(std::shared_ptr<MessengerToHdrPlusClient> messenger);

    // Messenger to send CaptureResult to the client.
    std::shared_ptr<MessengerToHdrPlusClient> mMessengerToClient;
};

} // namespace pbcamera

#endif // PAINTBOX_HDR_PLUS_PIPELINE_CAPTURE_RESULT_BLOCK_H
