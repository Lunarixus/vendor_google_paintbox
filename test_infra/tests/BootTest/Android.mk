LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := easel_boot_test
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE_OWNER := google
LOCAL_VENDOR_MODULE := true
LOCAL_CFLAGS += -UNDEBUG
LOCAL_SRC_FILES := TestEaselControlClient.cpp
LOCAL_C_INCLUDES := external/googletest/googletest/include
LOCAL_STATIC_LIBRARIES := libgtest
LOCAL_SHARED_LIBRARIES := libeaselcontrol.amber libcutils liblog
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := easel_boot_test_server
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE_OWNER := google
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/bin
LOCAL_CFLAGS += -UNDEBUG -DEASEL_SERVER
LOCAL_SRC_FILES := TestEaselControlServer.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES := libeaselcontrol.amber liblog
include $(BUILD_EXECUTABLE)
