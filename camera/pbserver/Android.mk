#
# pbserver
#
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS += -Wall -Wextra -Werror

LOCAL_SRC_FILES:= pbserver.cpp

LOCAL_C_INCLUDES += \
    hardware/google/paintbox/camera/include \
    hardware/google/paintbox/camera/libhdrplusmessenger/include \
    vendor/google_paintbox/include \
    vendor/google_paintbox/camera/services/include

LOCAL_SHARED_LIBRARIES:= libhdrplusservice

LOCAL_MODULE_OWNER := google

LOCAL_MODULE:= pbserver

LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_EXECUTABLE)
