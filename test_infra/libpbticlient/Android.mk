LOCAL_PATH:= $(call my-dir)

#
# libPbTiclient
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES :=  \
    PbTiClient.cpp

LOCAL_SHARED_LIBRARIES:= \
    libpbtimessenger \
    liblog \
    libutils \
    libcutils \
    libeaselcontrol.amber \
    libeaselcomm

LOCAL_EXPORT_SHARED_LIBRARY_HEADERS += libeaselcontrol.amber libutils

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/include \
    hardware/libhardware/include \
    system/core/include \
    vendor/google_paintbox/test_infra/include

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include \
                               vendor/google_paintbox/test_infra/include

LOCAL_CFLAGS += -Wall -Wextra -Werror

LOCAL_MODULE:= libpbticlient
LOCAL_MODULE_OWNER := google
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
