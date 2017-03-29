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
    libcutils

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/include \
    hardware/libhardware/include \
    system/core/include \
    vendor/google_paintbox/test_infra/include

ifeq ($(USE_LIB_EASEL), 0)
    LOCAL_STATIC_LIBRARIES := libmockeasel
    LOCAL_EXPORT_STATIC_LIBRARY_HEADERS := libmockeasel
    LOCAL_CFLAGS += -DUSE_LIB_EASEL=0
else
    LOCAL_STATIC_LIBRARIES := libeasel
    LOCAL_EXPORT_STATIC_LIBRARY_HEADERS := libeasel
    LOCAL_CFLAGS += -DUSE_LIB_EASEL=1
endif

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include \
                               vendor/google_paintbox/test_infra/include

LOCAL_CFLAGS += -Wall -Wextra -Werror

LOCAL_MODULE:= libpbticlient

include $(BUILD_SHARED_LIBRARY)
