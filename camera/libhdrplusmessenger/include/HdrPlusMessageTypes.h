/*
 * Copyright 2016 The Android Open Source Project
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
#ifndef PAINTBOX_HDR_PLUS_MESSAGE_TYPES_H
#define PAINTBOX_HDR_PLUS_MESSAGE_TYPES_H

#include <stdint.h>

#include "HdrPlusTypes.h"

namespace pbcamera {

// This file defines the types used in messages passed by EaselMessenger.


/*
 * DmaImageBuffer defines an image buffer that is ready to be transferred via DMA.
 */
struct DmaImageBuffer {
    // ID of the stream that this buffer belongs to.
    uint32_t streamId;
    // DMA handle that can be used to call transferDmaBuffer to transfer a DMA buffer.
    void* dmaHandle;
    // Size of the buffer to be transferred in bytes.
    uint32_t dmaDataSize;
};

/*
 * DmaCaptureResult defines a capture result that contains an image buffer that is ready to be
 * transferred via DMA. Upon receiving DmaCaptureResult, transferDmaBuffer must be called to
 * transfer the DMA buffer before returning from
 * MessengerListenerFromHdrPlusService::dmaCaptureResult.
 */
struct DmaCaptureResult {
    // ID of the CaptureRequest that this result corresponds to.
    uint32_t requestId;
    // DMA buffers that are ready to be transferred.
    DmaImageBuffer buffer;

    ResultMetadata metadata;
};

// Maximum message size passed between HDR+ client and service. 5KB for metadata.
const int kMaxHdrPlusMessageSize = 5120;

/*
 * HdrPlusMessageType defines the message types that can be passed between HDR+ service and
 * HDR+ client.
 */
enum HdrPlusMessageType {
    // Messages from HDR+ client to HDR+ service
    MESSAGE_CONNECT = 0,
    MESSAGE_DISCONNECT,
    MESSAGE_SET_STATIC_METADATA,
    MESSAGE_CONFIGURE_STREAMS,
    MESSAGE_SUBMIT_CAPTURE_REQUEST,
    MESSAGE_NOTIFY_DMA_INPUT_BUFFER,
    MESSAGE_NOTIFY_FRAME_METADATA_ASYNC,
    MESSAGE_SET_ZSL_HDR_PLUS_MODE,

    // Messages from HDR+ service to HDR+ client
    MESSAGE_NOTIFY_FRAME_EASEL_TIMESTAMP_ASYNC = 0x10000,
    MESSAGE_NOTIFY_DMA_CAPTURE_RESULT,
};

} // namespace pbcamera

#endif // PAINTBOX_HDR_PLUS_MESSAGE_TYPES_H
