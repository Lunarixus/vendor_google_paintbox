LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := easel_control_test
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE_OWNER := google
LOCAL_SRC_FILES := EaselControlTest.cpp
LOCAL_SHARED_LIBRARIES := libeaselcontrol libeaselcomm liblog libcutils
include $(BUILD_NATIVE_TEST)

include $(CLEAR_VARS)
LOCAL_MODULE := easel_control_test_server
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE_OWNER := google
LOCAL_SRC_FILES := EaselControlTestServer.cpp
LOCAL_SHARED_LIBRARIES := libeaselcontrol libeaselcomm liblog
include $(BUILD_NATIVE_TEST)

include $(CLEAR_VARS)
LOCAL_MODULE := EaselStateManagerTest
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE_OWNER := google
LOCAL_SRC_FILES := EaselStateManagerTest.cpp
LOCAL_SHARED_LIBRARIES := libeaselcontrol liblog
include $(BUILD_NATIVE_TEST)
