#
# imxserver
#
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CFLAGS += \
    -Wall -Wextra -Werror \
    -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS \
    -Wthread-safety
LOCAL_SRC_FILES := imxserver.cpp
LOCAL_SHARED_LIBRARIES += \
    libbase \
    libimxservice \
    libprotobuf-cpp-lite
LOCAL_STATIC_LIBRARIES += \
    libimxproto
LOCAL_C_INCLUDES += \
    vendor/google_paintbox/easel/kernel-headers
LOCAL_MODULE_OWNER := google
LOCAL_MODULE := imxserver
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := google
include $(BUILD_EXECUTABLE)
