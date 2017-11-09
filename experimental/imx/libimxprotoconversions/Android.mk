LOCAL_PATH:= $(call my-dir)

#
# libimxprotoconversions
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES :=  \
    ImxProtoConversions.cpp

LOCAL_SHARED_LIBRARIES := \
    libprotobuf-cpp-lite \
    liblog

LOCAL_STATIC_LIBRARIES := \
    libimxproto

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/include \
    vendor/google_paintbox/experimental/imx/libimx/include \
    vendor/google_paintbox/experimental/imx/libimxprotoconversions/include

LOCAL_EXPORT_C_INCLUDE_DIRS += \
    $(LOCAL_PATH)/include \
    vendor/google_paintbox/experimental/imx/libimx/include \
    vendor/google_paintbox/experimental/imx/libimxprotoconversions/include

LOCAL_CFLAGS += -Wall -Wextra -Werror
LOCAL_MODULE_OWNER := google

LOCAL_MODULE:= libimxprotoconversions
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_STATIC_LIBRARY)
