#ifndef PAINTBOX_HDR_PLUS_PIPELINE_DUMMY_PROCESSING_BLOCK_H
#define PAINTBOX_HDR_PLUS_PIPELINE_DUMMY_PROCESSING_BLOCK_H

#include "PipelineBuffer.h"
#include "PipelineBlock.h"

namespace pbcamera {

/*
 * DummyProcessingBlock is a pipeline block that processes a buffer in PipelineBlock::Input and
 * writes the result image to buffers in PipelineBlock::OutputRequest. DummyProcessingBlock's
 * doWorkLocked() starts its work when a PipelineBlock::Input and a PipelineBlock::OutputRequest is
 * available.
 *
 * DummyProcessingBlock is just a placeholder to test the pipeline buffer flow and doesn't do any
 * useful processing.
 */
class DummyProcessingBlock : public PipelineBlock {
public:
    virtual ~DummyProcessingBlock();

    /*
     * Create a DummyProcessingBlock.
     *
     * pipeline is the pipline this block belongs to.
     *
     * Returns a std::shared_ptr<DummyProcessingBlock> pointing to a DummyProcessingBlock on
     *         success.
     * Returns a std::shared_ptr<DummyProcessingBlock> pointing to nullptr if it failed.
     */
    static std::shared_ptr<DummyProcessingBlock> newDummyProcessingBlock(
                std::weak_ptr<HdrPlusPipeline> pipeline);
    bool doWorkLocked() override;
private:
    // Use newDummyProcessingBlock to create a DummyProcessingBlock.
    DummyProcessingBlock();

    // Dummy processing just sets every byte of the output buffer to this value.
    const uint8_t kSetByteValue = 0x5;
};

} // namespace pbcamera

#endif // PAINTBOX_HDR_PLUS_PIPELINE_DUMMY_PROCESSING_BLOCK_H
