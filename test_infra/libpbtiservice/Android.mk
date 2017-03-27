LOCAL_PATH:= $(call my-dir)

#
# libPbTiservice
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES :=  \
    PbTiService.cpp

LOCAL_SHARED_LIBRARIES:= \
    libpbtimessenger \
    liblog

ifeq ($(USE_LIB_EASEL), 0)
    LOCAL_STATIC_LIBRARIES := libmockeasel
else
    LOCAL_STATIC_LIBRARIES := libeasel
endif

LOCAL_C_INCLUDES += \
    vendor/google_paintbox/libeasel/include \
    vendor/google_paintbox/test_infra/libpbtiservice/include

LOCAL_CFLAGS += -Wall -Wextra -Werror

LOCAL_MODULE:= libpbtiservice

include $(BUILD_SHARED_LIBRARY)
