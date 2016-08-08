# Copyright 2016 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
PB_AOSP_INCLUDES := $(TOP)/hardware/google/paintbox/include

include $(CLEAR_VARS)
LOCAL_MODULE := ezlsh
LOCAL_CFLAGS += -Wall -Werror -UNDEBUG
LOCAL_SRC_FILES := ezlsh.cpp
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(PB_AOSP_INCLUDES)
LOCAL_STATIC_LIBRARIES := libeasel
include $(BUILD_EXECUTABLE)

