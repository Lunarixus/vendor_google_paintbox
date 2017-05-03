# Copyright 2016 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
PB_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/../kernel-headers

# Client side libeasel, it contains server side code
# for build requirement.
include $(CLEAR_VARS)
LOCAL_MODULE := libeaselcontrol
LOCAL_MODULE_OWNER := google
LOCAL_C_INCLUDES := $(PB_INCLUDES)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(PB_INCLUDES)
LOCAL_CPPFLAGS += -Wall -Werror -UNDEBUG
LOCAL_SRC_FILES := \
    EaselClockControl.cpp \
    EaselControlClient.cpp \
    EaselControlServer.cpp \
    EaselStateManager.cpp \
    EaselThermalMonitor.cpp \
    ThermalZone.cpp
LOCAL_SHARED_LIBRARIES := libcutils liblog libeaselcomm
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)

# Server side libeasel, will be rename to libeasel
# and installed on server side.
include $(CLEAR_VARS)
LOCAL_MODULE := libeaselcontrolservice
LOCAL_MODULE_OWNER := google
LOCAL_C_INCLUDES := $(PB_INCLUDES)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(PB_INCLUDES)
LOCAL_CPPFLAGS += -Wall -Werror -UNDEBUG -DEASELSERVER
LOCAL_SRC_FILES := \
    EaselClockControl.cpp \
    EaselControlServer.cpp \
    EaselThermalMonitor.cpp \
    ThermalZone.cpp
LOCAL_SHARED_LIBRARIES := libeaselcomm
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)
