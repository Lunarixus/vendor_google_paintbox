#
# pbserver
#
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS += -Wall -Wextra -Werror

LOCAL_SRC_FILES:= pbserver.cpp

LOCAL_C_INCLUDES += \
    hardware/google/paintbox/kernel-headers \
    vendor/google_paintbox/camera/include \
    vendor/google_paintbox/camera/services/include \
    vendor/google_paintbox/camera/libhdrplusmessenger/include \
    vendor/google_paintbox/libeasel/include

LOCAL_SHARED_LIBRARIES:= libhdrplusservice

LOCAL_MODULE_OWNER := google

LOCAL_MODULE:= pbserver

LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := google

include $(BUILD_EXECUTABLE)
