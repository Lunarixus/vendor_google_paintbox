# Copyright 2016 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := ezlsh
LOCAL_MODULE_OWNER := google
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/bin
LOCAL_CFLAGS += -Wall -Werror -UNDEBUG
LOCAL_SRC_FILES := ezlsh.cpp
#LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES := libeasel
include $(BUILD_EXECUTABLE)

