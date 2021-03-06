LOCAL_PATH:= $(call my-dir)

#
# libpbtimessenger
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES :=  \
    EaselMessenger.cpp \
    MessengerToPbTiClient.cpp \
    MessengerListenerFromPbTiClient.cpp \
    MessengerToPbTiService.cpp \
    MessengerListenerFromPbTiService.cpp

LOCAL_SHARED_LIBRARIES:= \
    libutils \
    liblog \
    libcutils \
    libeaselcomm

LOCAL_EXPORT_SHARED_LIBRARY_HEADERS += libeaselcomm

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/include \
    vendor/google_paintbox/test_infra/include

LOCAL_EXPORT_C_INCLUDE_DIRS += \
    $(LOCAL_PATH)/include \
    vendor/google_paintbox/test_infra/include


LOCAL_CFLAGS += -Wall -Wextra -Werror

LOCAL_MODULE:= libpbtimessenger
LOCAL_MODULE_OWNER := google
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
