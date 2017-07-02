# Copyright 2016 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := ezlsh
LOCAL_MODULE_OWNER := google
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/bin
LOCAL_CFLAGS += -Wall -Werror -UNDEBUG
LOCAL_SRC_FILES := ezlsh.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES := libeaselcomm
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := ezltmon
LOCAL_MODULE_OWNER := google
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/bin
LOCAL_CFLAGS += -Wall -Werror -UNDEBUG
LOCAL_SRC_FILES := ezltmon.cpp
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := ezlspi
LOCAL_MODULE_OWNER := google
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/bin
LOCAL_CFLAGS += -Wall -Werror -UNDEBUG
LOCAL_SRC_FILES := ezlspi.cpp
include $(BUILD_EXECUTABLE)

