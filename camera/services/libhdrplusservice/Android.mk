LOCAL_PATH:= $(call my-dir)

#
# libhdrplusservice
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES :=  \
    blocks/CaptureResultBlock.cpp \
    blocks/HdrPlusProcessingBlock.cpp \
    blocks/PipelineBlock.cpp \
    blocks/SourceCaptureBlock.cpp \
    HdrPlusPipeline.cpp \
    HdrPlusService.cpp \
    PipelineBuffer.cpp \
    PipelineStream.cpp

LOCAL_SHARED_LIBRARIES:= \
    libgcam_pb \
    libhdrplusmessenger \
    liblog

LOCAL_C_INCLUDES += \
    hardware/google/paintbox/include \
    hardware/google/paintbox/camera/include \
    system/media/camera/include \
    vendor/google_paintbox/include \
    vendor/google_paintbox/camera/services/include

LOCAL_CFLAGS += -Wall -Wextra -Werror

LOCAL_MODULE:= libhdrplusservice

include $(BUILD_SHARED_LIBRARY)
