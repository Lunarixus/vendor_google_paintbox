#
# pbtiserver
#
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS += -Wall -Wextra -Werror

LOCAL_SRC_FILES:= pbtiserver.cpp

LOCAL_C_INCLUDES += \
    hardware/google/paintbox/kernel-headers \
    vendor/google_paintbox/libeasel/include \
    vendor/google_paintbox/test_infra/include \
    vendor/google_paintbox/test_infra/libpbtimessenger/include \
    vendor/google_paintbox/test_infra/libpbtiservice/include

LOCAL_SHARED_LIBRARIES:= libpbtiservice \
                         liblog

LOCAL_MODULE:= pbtiserver

include $(BUILD_EXECUTABLE)
