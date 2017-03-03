LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := log.cpp

LOCAL_SHARED_LIBRARIES := libeasel

LOCAL_CFLAGS += -Wall -Wextra -Werror

# liblog on easel.
# Named as libeasellog to avoid name conflict.
# This library may be later moved out of Android source tree.
LOCAL_MODULE:= libeasellog

include $(BUILD_SHARED_LIBRARY)
