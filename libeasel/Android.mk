# Copyright 2016 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
PB_AOSP_INCLUDES := $(TOP)/hardware/google/paintbox/include \
	$(TOP)/hardware/google/paintbox/kernel-headers

include $(CLEAR_VARS)
LOCAL_MODULE := libeasel
LOCAL_C_INCLUDES := $(PB_AOSP_INCLUDES)
LOCAL_CPPFLAGS += -Wall -Werror -UNDEBUG
LOCAL_SRC_FILES := \
	EaselComm.cpp
include $(BUILD_STATIC_LIBRARY)
