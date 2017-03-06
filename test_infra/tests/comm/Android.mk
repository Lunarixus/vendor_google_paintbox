LOCAL_PATH:= $(call my-dir)

PB_AOSP_INCLUDES += \
    $(LOCAL_PATH)/include \
    hardware/google/paintbox/include \
    hardware/google/paintbox/kernel-headers

include $(CLEAR_VARS)
LOCAL_MODULE := pbti_easelcomm_test_client
LOCAL_MODULE_TAGS := tests
LOCAL_CFLAGS += -UNDEBUG -DAP_CLIENT
LOCAL_SRC_FILES := easelcomm_test.cpp
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_C_INCLUDES := $(PB_AOSP_INCLUDES)
LOCAL_STATIC_LIBRARIES := libeasel liblog
include $(BUILD_NATIVE_TEST)

include $(CLEAR_VARS)
LOCAL_MODULE := pbti_easelcomm_test_server
LOCAL_MODULE_TAGS := tests
LOCAL_CFLAGS += -UNDEBUG -DEASEL_SERVER
LOCAL_SRC_FILES := easelcomm_test.cpp
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_C_INCLUDES := $(PB_AOSP_INCLUDES)
LOCAL_STATIC_LIBRARIES := libeasel liblog
include $(BUILD_NATIVE_TEST)
