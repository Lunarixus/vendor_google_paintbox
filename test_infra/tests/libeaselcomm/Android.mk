LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := easelcomm_test
LOCAL_MODULE_TAGS := tests
LOCAL_CFLAGS += -UNDEBUG -DAP_CLIENT
LOCAL_SRC_FILES := easelcomm_test.cpp
LOCAL_SHARED_LIBRARIES := libeaselcomm liblog
include $(BUILD_NATIVE_TEST)

include $(CLEAR_VARS)
LOCAL_MODULE := easelcomm_test_server
LOCAL_MODULE_TAGS := tests
LOCAL_CFLAGS += -UNDEBUG -DEASEL_SERVER
LOCAL_SRC_FILES := easelcomm_test.cpp
LOCAL_SHARED_LIBRARIES := libeaselcomm liblog
include $(BUILD_NATIVE_TEST)

include $(CLEAR_VARS)
LOCAL_MODULE := easelcomm2_impl_test
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE_OWNER := google
LOCAL_SRC_FILES := EaselComm2ImplTest.cpp
LOCAL_SHARED_LIBRARIES := libnativewindow libeaselcomm
include $(BUILD_NATIVE_TEST)

include $(CLEAR_VARS)
LOCAL_MODULE := easelcomm2_impl_test_server
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE_OWNER := google
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SRC_FILES := EaselComm2ImplTestServer.cpp
LOCAL_SHARED_LIBRARIES := libeaselcomm libimageprocessor libbase
include $(BUILD_EXECUTABLE)
