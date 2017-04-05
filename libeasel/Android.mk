# Copyright 2016 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
PB_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/kernel-headers

include $(CLEAR_VARS)
LOCAL_MODULE := libeasel
LOCAL_MODULE_OWNER := google
LOCAL_C_INCLUDES := $(PB_INCLUDES)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(PB_INCLUDES)
LOCAL_CPPFLAGS += -Wall -Werror -UNDEBUG
LOCAL_SRC_FILES := \
    EaselClockControl.cpp \
    EaselComm.cpp \
    EaselControlClient.cpp \
    EaselControlServer.cpp \
    EaselStateManager.cpp
# To avoid circular dependency between libeasel
# liblog (libeasellog) on server side, liblog is
# statically linked in libeasel for client usage.
# For logging on libeasel server, please use
# easelLog directly instead of liblog.
LOCAL_STATIC_LIBRARIES := liblog
LOCAL_SHARED_LIBRARIES := libcutils
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := easelcomm_test_client
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE_OWNER := google
LOCAL_CFLAGS += -UNDEBUG -DAP_CLIENT
LOCAL_SRC_FILES := easelcomm_test.cpp
LOCAL_SHARED_LIBRARIES := libeasel liblog
include $(BUILD_NATIVE_TEST)

include $(CLEAR_VARS)
LOCAL_MODULE := easelcomm_test_server
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE_OWNER := google
LOCAL_CFLAGS += -UNDEBUG -DEASEL_SERVER
LOCAL_SRC_FILES := easelcomm_test.cpp
LOCAL_SHARED_LIBRARIES := libeasel liblog
include $(BUILD_NATIVE_TEST)

include $(CLEAR_VARS)
LOCAL_MODULE := libmockeasel
LOCAL_MODULE_OWNER := google
LOCAL_C_INCLUDES := $(PB_INCLUDES)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(PB_INCLUDES)
LOCAL_CPPFLAGS += -Wall -Werror -UNDEBUG -DMOCKEASEL
LOCAL_SRC_FILES := \
    EaselClockControl.cpp \
    EaselCommNet.cpp \
    EaselControlClient.cpp \
    EaselControlServer.cpp \
    EaselStateManager.cpp
# See explanation in libeasel
LOCAL_STATIC_LIBRARIES := liblog
LOCAL_SHARED_LIBRARIES := libcutils
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)
