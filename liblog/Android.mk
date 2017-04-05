LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	log.cpp \
	log_stub.cpp

LOCAL_STATIC_LIBRARIES := libdebuggerd

LOCAL_SHARED_LIBRARIES := libunwind liblzma libeasel libbacktrace libbase

LOCAL_CFLAGS += -Wall -Wextra -Werror

# liblog on easel.
# Named as libeasellog to avoid name conflict.
# This library may be later moved out of Android source tree.
LOCAL_MODULE_OWNER := google

LOCAL_MODULE := libeasellog

LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)

LOCAL_SRC_FILES := test/log_test.cpp

LOCAL_SHARED_LIBRARIES := liblog

LOCAL_CFLAGS += -Wall -Wextra -Werror -std=c++11

LOCAL_MODULE_OWNER := google

LOCAL_MODULE := libeasellog_test
LOCAL_MODULE_TAGS := tests

include $(BUILD_NATIVE_TEST)
