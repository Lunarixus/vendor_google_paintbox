# Copyright 2016 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
PB_AOSP_INCLUDES := $(TOP)/hardware/google/paintbox/include \
	$(TOP)/hardware/google/paintbox/kernel-headers

include $(CLEAR_VARS)
LOCAL_MODULE := libeasel
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(PB_AOSP_INCLUDES)
LOCAL_CPPFLAGS += -Wall -Werror -UNDEBUG
LOCAL_SRC_FILES := \
	EaselComm.cpp \
	EaselControlClient.cpp \
	EaselControlServer.cpp
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := easelcomm_test_client
LOCAL_MODULE_TAGS := tests
LOCAL_CFLAGS += -UNDEBUG -DAP_CLIENT
LOCAL_SRC_FILES := easelcomm_test.cpp
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_C_INCLUDES := $(PB_AOSP_INCLUDES)
LOCAL_STATIC_LIBRARIES := libeasel liblog
include $(BUILD_NATIVE_TEST)

include $(CLEAR_VARS)
LOCAL_MODULE := easelcomm_test_server
LOCAL_MODULE_TAGS := tests
LOCAL_CFLAGS += -UNDEBUG -DEASEL_SERVER
LOCAL_SRC_FILES := easelcomm_test.cpp
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_C_INCLUDES := $(PB_AOSP_INCLUDES)
LOCAL_STATIC_LIBRARIES := libeasel liblog
include $(BUILD_NATIVE_TEST)

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
LOCAL_MODULE := mockeaselcomm_test
LOCAL_MODULE_TAGS := tests
LOCAL_CFLAGS += -UNDEBUG -DMOCKEASEL
LOCAL_SRC_FILES := easelcomm_test.cpp
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_C_INCLUDES := $(PB_AOSP_INCLUDES)
LOCAL_STATIC_LIBRARIES := libmockeasel liblog
include $(BUILD_NATIVE_TEST)
