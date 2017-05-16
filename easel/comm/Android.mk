# Copyright 2016 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
PB_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/../kernel-headers

include $(CLEAR_VARS)
LOCAL_MODULE := libeaselcomm
LOCAL_MODULE_OWNER := google
LOCAL_C_INCLUDES := $(PB_INCLUDES)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(PB_INCLUDES)
LOCAL_CPPFLAGS += -Wall -Werror -UNDEBUG
LOCAL_SRC_FILES := EaselComm.cpp
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libmockeaselcomm
LOCAL_MODULE_OWNER := google
LOCAL_C_INCLUDES := $(PB_INCLUDES)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(PB_INCLUDES)
LOCAL_CPPFLAGS += -Wall -Werror -UNDEBUG -DMOCKEASEL
LOCAL_SRC_FILES := EaselCommNet.cpp
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)