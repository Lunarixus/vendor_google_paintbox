#ifndef PAINTBOX_IMX_CHANNELS_H
#define PAINTBOX_IMX_CHANNELS_H

namespace imx {

enum ImxChannel {
    kGetDefaultDeviceChannel = 0,
    kDefaultCreateTransferNodeInfoChannel,
    kCreateTransferNodeChannel,
    kCreateGraphChannel,
    kCompileGraphChannel,
    kDeleteGraphChannel,
    kCreateJobChannel,
    kCreateDeviceBufferSimpleChannel,
    kDeleteDeviceChannel,
    kFinalizeBuffersChannel,
    kLockDeviceBufferChannel,
    kUnlockDeviceBufferChannel,
    kExecuteJobChannel,
    kExecuteFinishJobChannel
};

}  // namespace imx

#endif // PAINTBOX_IMX_CHANNELS_H
