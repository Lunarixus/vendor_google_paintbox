# Copyright 2016 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
PB_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/kernel-headers

# Client side libeasel, it contains server side code
# for build requirement.
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
    EaselStateManager.cpp \
    EaselThermalMonitor.cpp \
    ThermalZone.cpp
LOCAL_SHARED_LIBRARIES := libcutils liblog
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)

# Server side libeasel, will be rename to libeasel
# and installed on server side.
include $(CLEAR_VARS)
LOCAL_MODULE := libeaselservice
LOCAL_MODULE_OWNER := google
LOCAL_C_INCLUDES := $(PB_INCLUDES)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(PB_INCLUDES)
LOCAL_CPPFLAGS += -Wall -Werror -UNDEBUG -DEASELSERVER
LOCAL_SRC_FILES := \
    EaselClockControl.cpp \
    EaselComm.cpp \
    EaselControlServer.cpp \
    EaselThermalMonitor.cpp \
    ThermalZone.cpp
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)

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
    EaselStateManager.cpp \
    EaselThermalMonitor.cpp \
    ThermalZone.cpp
LOCAL_SHARED_LIBRARIES := liblog libcutils
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)
