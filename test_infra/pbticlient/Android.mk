LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= pbticlient.cpp

LOCAL_SHARED_LIBRARIES := \
    libpbticlient \
    libpbtimessenger \
    liblog

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/include \
    vendor/google_paintbox/test_infra/include

LOCAL_CFLAGS += -Wall -Wextra -Werror -Wno-unused-parameter

LOCAL_MODULE:= pbticlient
LOCAL_MODULE_OWNER := google
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_EXECUTABLE)
