LOCAL_PATH:= $(call my-dir)

#
# libimxservice
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES :=  \
    ImxService.cpp

LOCAL_SHARED_LIBRARIES := \
    libbase \
    libeaselcomm \
    libimageprocessor \
    libprotobuf-cpp-lite \
    liblog

LOCAL_STATIC_LIBRARIES := \
    libimxcommon \
    libimxproto \
    libimxprotoconversions

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/include \
    vendor/google_paintbox/experimental/imx/libimx/include \
    vendor/google_paintbox/experimental/imx/libimxprotoconversions/include

LOCAL_EXPORT_C_INCLUDE_DIRS += \
    $(LOCAL_PATH)/include \
    vendor/google_paintbox/experimental/imx/libimx/include \
    vendor/google_paintbox/experimental/imx/libimxcommon/include \
    vendor/google_paintbox/easel/comm/include

LOCAL_CFLAGS += \
    -Wall -Wextra -Werror \
    -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS \
    -Wthread-safety
LOCAL_MODULE_OWNER := google

LOCAL_MODULE:= libimxservice
LOCAL_MULTILIB := 64
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
