LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := easelcomm_test
LOCAL_MODULE_TAGS := tests
LOCAL_CFLAGS += -UNDEBUG -DAP_CLIENT
LOCAL_SRC_FILES := easelcomm_test.cpp
LOCAL_SHARED_LIBRARIES := libeasel liblog
include $(BUILD_NATIVE_TEST)

include $(CLEAR_VARS)
LOCAL_MODULE := easelcomm_test_server
LOCAL_MODULE_TAGS := tests
LOCAL_CFLAGS += -UNDEBUG -DEASEL_SERVER
LOCAL_SRC_FILES := easelcomm_test.cpp
LOCAL_SHARED_LIBRARIES := libeasel liblog
include $(BUILD_NATIVE_TEST)

include $(CLEAR_VARS)
LOCAL_MODULE := easel_control_test
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE_OWNER := google
LOCAL_SRC_FILES := EaselControlTest.cpp
LOCAL_SHARED_LIBRARIES := libeasel liblog libcutils
include $(BUILD_NATIVE_TEST)

include $(CLEAR_VARS)
LOCAL_MODULE := easel_control_test_server
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE_OWNER := google
LOCAL_SRC_FILES := EaselControlTestServer.cpp
LOCAL_SHARED_LIBRARIES := libeasel liblog
include $(BUILD_NATIVE_TEST)

include $(CLEAR_VARS)
LOCAL_MODULE := EaselStateManagerTest
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE_OWNER := google
LOCAL_SRC_FILES := EaselStateManagerTest.cpp
LOCAL_SHARED_LIBRARIES := libeasel liblog
include $(BUILD_NATIVE_TEST)
