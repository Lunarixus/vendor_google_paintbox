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
    hardware/google/paintbox/include \
    vendor/google_paintbox/test_infra/include \
    hardware/libhardware/include \
    system/core/include

ifeq ($(USE_LIB_EASEL), 1)
       LOCAL_STATIC_LIBRARIES := libeasel
       LOCAL_CFLAGS += -DUSE_LIB_EASEL=1
else
       LOCAL_STATIC_LIBRARIES := libmockeasel
       LOCAL_CFLAGS += -DUSE_LIB_EASEL=0
endif

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include \
                               vendor/google_paintbox/include \
                               vendor/google_paintbox/test_infra/include

LOCAL_CFLAGS += -Wall -Wextra -Werror

LOCAL_MODULE:= libpbticlient

include $(BUILD_SHARED_LIBRARY)
