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
    libeaselsystem \
    libeaselcomm \
    libeaselcontrol \
    libgcam \
    libhdrplusmessenger \
    liblog \
    libimageprocessor

LOCAL_HEADER_LIBRARIES := \
    libsystem_headers

LOCAL_EXPORT_SHARED_LIBRARY_HEADERS += libeaselcontrol

LOCAL_C_INCLUDES += \
    hardware/google/easel/camera/include \
    system/media/camera/include \
    vendor/google_paintbox/camera/include \
    vendor/google_paintbox/camera/services/include

LOCAL_EXPORT_C_INCLUDE_DIRS += \
    vendor/google_paintbox/camera/services/include

LOCAL_EXPORT_SHARED_LIBRARY_HEADERS += libhdrplusmessenger

LOCAL_CFLAGS += -Wall -Wextra -Werror

# These are needed to ignore warnings from third_party/halide/halide/src/runtime/HalideBuffer.h
LOCAL_CFLAGS += -Wno-missing-field-initializers -Wno-unused-parameter

LOCAL_MODULE_OWNER := google

LOCAL_MODULE:= libhdrplusservice
LOCAL_MULTILIB := 64
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
