#
# pbtiserver
#
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS += -Wall -Wextra -Werror

LOCAL_SRC_FILES:= pbtiserver.cpp

LOCAL_C_INCLUDES += \
    vendor/google_paintbox/test_infra/include \

LOCAL_SHARED_LIBRARIES:= libpbtiservice \
                         liblog

LOCAL_MODULE:= pbtiserver
LOCAL_MODULE_OWNER := google
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_EXECUTABLE)
