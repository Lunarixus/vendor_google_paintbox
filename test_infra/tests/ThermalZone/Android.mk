LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := ThermalZoneTest
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE_OWNER := google
LOCAL_CFLAGS += -UNDEBUG
LOCAL_SRC_FILES := ThermalZoneTest.cpp
LOCAL_SHARED_LIBRARIES := libeaselcontrol liblog
include $(BUILD_NATIVE_TEST)