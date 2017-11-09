LOCAL_PATH:= $(call my-dir)

#
# libimxcommon
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES :=  \
    ImxUtils.cpp

LOCAL_SHARED_LIBRARIES := \
    liblog

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/include

LOCAL_EXPORT_C_INCLUDE_DIRS += \
    $(LOCAL_PATH)/include

LOCAL_CFLAGS += -Wall -Wextra -Werror
LOCAL_MODULE_OWNER := google

LOCAL_MODULE:= libimxcommon
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_STATIC_LIBRARY)
