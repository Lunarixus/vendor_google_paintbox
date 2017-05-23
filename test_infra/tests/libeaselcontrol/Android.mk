LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := EaselStateManagerTest
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE_OWNER := google
LOCAL_SRC_FILES := EaselStateManagerTest.cpp
LOCAL_SHARED_LIBRARIES := libeaselcontrol liblog
include $(BUILD_NATIVE_TEST)
