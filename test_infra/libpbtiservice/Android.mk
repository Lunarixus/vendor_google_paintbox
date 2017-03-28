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
    LOCAL_EXPORT_STATIC_LIBRARY_HEADERS := libmockeasel
else
    LOCAL_STATIC_LIBRARIES := libeasel
    LOCAL_EXPORT_STATIC_LIBRARY_HEADERS := libeasel
endif

LOCAL_C_INCLUDES += $(LOCAL_PATH)/include

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include

LOCAL_EXPORT_SHARED_LIBRARY_HEADERS := libpbtimessenger

LOCAL_CFLAGS += -Wall -Wextra -Werror

LOCAL_MODULE:= libpbtiservice

include $(BUILD_SHARED_LIBRARY)
