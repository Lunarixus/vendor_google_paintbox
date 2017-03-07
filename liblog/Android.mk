LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	log.cpp \
	log_stub.cpp

LOCAL_STATIC_LIBRARIES := libeasel libdebuggerd libbacktrace libbase libunwind liblzma

LOCAL_CFLAGS += -Wall -Wextra -Werror

# liblog on easel.
# Named as libeasellog to avoid name conflict.
# This library may be later moved out of Android source tree.
LOCAL_MODULE:= libeasellog

LOCAL_C_INCLUDES := $(DEBUGGERD_INCLUDES)

include $(BUILD_SHARED_LIBRARY)