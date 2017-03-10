#
# pbtiserver
#
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS += -Wall -Wextra -Werror

LOCAL_SRC_FILES:= pbtiserver.cpp

LOCAL_C_INCLUDES += \
    vendor/google_paintbox/test_infra/include \
    vendor/google_paintbox/test_infra/libpbtimessenger/include \
    vendor/google_paintbox/include \
    vendor/google_paintbox/test_infra/libpbtiservice/include

LOCAL_SHARED_LIBRARIES:= libpbtiservice \
                         liblog

LOCAL_MODULE:= pbtiserver

include $(BUILD_EXECUTABLE)