LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE        := libimageprocessor
LOCAL_MODULE_CLASS  := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := .so
LOCAL_STRIP_MODULE  := false
LOCAL_MODULE_OWNER  := google
LOCAL_MODULE_TAGS   := optional
LOCAL_SRC_FILES_arm64       := libs/libimageprocessor.so
LOCAL_PROPRIETARY_MODULE    := true
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/libs/include
LOCAL_MULTILIB := 64
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE        := libcapture
LOCAL_MODULE_CLASS  := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := .so
LOCAL_STRIP_MODULE  := false
LOCAL_MODULE_OWNER  := google
LOCAL_MODULE_TAGS   := optional
LOCAL_SRC_FILES_arm64       := libs/libcapture.so
LOCAL_PROPRIETARY_MODULE    := true
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/libs/include
LOCAL_MULTILIB := 64
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE        := libgcam
LOCAL_MODULE_CLASS  := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := .so
LOCAL_STRIP_MODULE  := false
LOCAL_MODULE_OWNER  := google
LOCAL_MODULE_TAGS   := optional
LOCAL_SRC_FILES_arm64       := libs/libgcam.so
LOCAL_PROPRIETARY_MODULE    := true
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/libs/include
LOCAL_MULTILIB := 64
include $(BUILD_PREBUILT)
