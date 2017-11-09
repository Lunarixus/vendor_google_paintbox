LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := basic_imx_test
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE_OWNER := google
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/bin
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES := \
    BasicTest.cpp \
    GreyTestCommon.cpp \
    FinishJobTest.cpp
LOCAL_SHARED_LIBRARIES := \
    libbase \
    libimx \
    liblog
LOCAL_STATIC_LIBRARIES := \
    libgtest
include $(BUILD_EXECUTABLE)
