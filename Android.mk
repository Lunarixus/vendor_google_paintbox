# Copyright 2016 The Android Open Source Project

ifeq ($(TARGET_USES_PAINTBOX), true)

LOCAL_PATH:= $(call my-dir)
PB_AOSP_INCLUDES := $(TOP)/hardware/google/paintbox/include

include $(CLEAR_VARS)
LOCAL_MODULE := libmockeasel
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(PB_AOSP_INCLUDES)
LOCAL_CPPFLAGS += -Wall -Werror -UNDEBUG -DMOCKEASEL
LOCAL_SRC_FILES := \
	EaselCommNet.cpp \
	EaselControlClient.cpp \
	EaselControlServer.cpp
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := ezlsh
LOCAL_CFLAGS += -Wall -Werror -UNDEBUG -DMOCKEASEL
LOCAL_SRC_FILES := ezlsh.cpp
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(PB_AOSP_INCLUDES)
LOCAL_STATIC_LIBRARIES := libmockeasel
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := mockeaselcomm_test
LOCAL_MODULE_TAGS := tests
LOCAL_CFLAGS += -UNDEBUG -DMOCKEASEL
LOCAL_SRC_FILES := mockeaselcomm_test.cpp
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_C_INCLUDES := $(PB_AOSP_INCLUDES)
LOCAL_STATIC_LIBRARIES := libmockeasel liblog
include $(BUILD_NATIVE_TEST)

include $(call all-makefiles-under, $(call my-dir))

endif
