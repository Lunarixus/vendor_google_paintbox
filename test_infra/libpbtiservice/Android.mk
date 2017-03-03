LOCAL_PATH:= $(call my-dir)

#
# libPbTiservice
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES :=  \
    PbTiService.cpp

LOCAL_SHARED_LIBRARIES:= \
    libpbtimessenger \
    liblog

ifeq ($(USE_LIB_EASEL), 1)
    LOCAL_SHARED_LIBRARIES := libeasel
else
    LOCAL_SHARED_LIBRARIES := libmockeasel
endif

LOCAL_C_INCLUDES += \
    hardware/google/paintbox/include \
    vendor/google_paintbox/test_infra/include \
    vendor/google_paintbox/include \
    vendor/google_paintbox/test_infra/libpbtiservice/include

LOCAL_EXPORT_C_INCLUDE_DIRS := \
    hardware/google/paintbox/include \
    hardware/google/paintbox/kernel-headers

LOCAL_CFLAGS += -Wall -Wextra -Werror

LOCAL_MODULE:= libpbtiservice

include $(BUILD_SHARED_LIBRARY)
