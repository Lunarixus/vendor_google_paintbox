LOCAL_PATH:= $(call my-dir)

#
# libhdrplusservice
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES :=  \
    HdrPlusService.cpp

LOCAL_SHARED_LIBRARIES:= \
    libhdrplusmessenger \
    liblog

LOCAL_C_INCLUDES += \
    hardware/google/paintbox/include \
    hardware/google/paintbox/camera/include \
    hardware/google/paintbox/camera/libhdrplusmessenger/include \
    vendor/google_paintbox/include \
    vendor/google_paintbox/camera/services/include

LOCAL_CFLAGS += -Wall -Wextra -Werror

LOCAL_MODULE:= libhdrplusservice

include $(BUILD_SHARED_LIBRARY)
