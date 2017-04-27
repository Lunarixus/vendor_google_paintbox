LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE:= logd.easel

LOCAL_SRC_FILES := \
    main.cpp \
    LogBufferEasel.cpp

LOCAL_STATIC_LIBRARIES := \
    liblogd

LOCAL_SHARED_LIBRARIES := \
    libsysutils \
    liblog \
    libcutils \
    libbase \
    libeaselcomm

LOCAL_CFLAGS := -Werror

LOCAL_MODULE_OWNER := google
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_MODULE:= logdclient

LOCAL_SRC_FILES := LogClient.cpp

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libeaselcomm

LOCAL_CFLAGS := -Werror

LOCAL_MODULE_OWNER := google
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE:= logdclient.easel

LOCAL_SRC_FILES := LogClientDaemon.cpp

LOCAL_STATIC_LIBRARIES := \
    logdclient

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libeaselcomm

LOCAL_CFLAGS := -Werror

LOCAL_MODULE_OWNER := google
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_EXECUTABLE)