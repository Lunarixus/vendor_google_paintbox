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
LOCAL_MODULE := easelcomm2_impl_test_proto
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_PROTOC_OPTIMIZE_TYPE := lite
LOCAL_SRC_FILES := $(call all-proto-files-under)
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := easelcomm2_impl_test
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE_OWNER := google
LOCAL_SRC_FILES := EaselComm2ImplTest.cpp
LOCAL_SHARED_LIBRARIES := libnativewindow libeaselcomm libprotobuf-cpp-lite
LOCAL_STATIC_LIBRARIES := easelcomm2_impl_test_proto
include $(BUILD_NATIVE_TEST)

include $(CLEAR_VARS)
LOCAL_MODULE := easelcomm2_impl_test_server
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE_OWNER := google
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SRC_FILES := EaselComm2ImplTestServer.cpp
LOCAL_SHARED_LIBRARIES := libeaselcomm libimageprocessor libbase libprotobuf-cpp-lite
LOCAL_STATIC_LIBRARIES := easelcomm2_impl_test_proto
include $(BUILD_EXECUTABLE)
