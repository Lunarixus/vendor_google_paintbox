LOCAL_PATH:= $(call my-dir)

PB_AOSP_INCLUDES += \
    $(LOCAL_PATH)/include \
    hardware/google/paintbox/include \
    hardware/google/paintbox/kernel-headers

include $(CLEAR_VARS)
LOCAL_MODULE := EaselStateManagerTest
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE_OWNER := google
LOCAL_CFLAGS += -UNDEBUG
LOCAL_SRC_FILES := EaselStateManagerTest.cpp
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_C_INCLUDES := vendor/google_paintbox/libeasel/include $(PB_AOSP_INCLUDES)
LOCAL_STATIC_LIBRARIES := libeasel liblog
include $(BUILD_NATIVE_TEST)


