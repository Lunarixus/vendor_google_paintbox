# Copyright 2016 The Android Open Source Project

ifeq ($(TARGET_USES_PAINTBOX), true)

LOCAL_PATH:= $(call my-dir)
PB_AOSP_INCLUDES := $(TOP)/hardware/google_paintbox/include

include $(CLEAR_VARS)
LOCAL_MODULE := libmockeasel
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(PB_AOSP_INCLUDES)
LOCAL_CFLAGS += -Wall -Werror -UNDEBUG
LOCAL_SRC_FILES := \
	EaselCommNet.cpp
include $(BUILD_STATIC_LIBRARY)

include $(call all-makefiles-under, $(call my-dir))

endif
