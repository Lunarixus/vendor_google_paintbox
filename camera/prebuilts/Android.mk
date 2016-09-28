LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE        := libgcam_pb
LOCAL_MODULE_CLASS  := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := .so
LOCAL_STRIP_MODULE  := false
LOCAL_MODULE_OWNER  := google
LOCAL_MODULE_TAGS   := optional
LOCAL_SRC_FILES_arm         := gcam/lib/libgcam.so
LOCAL_SRC_FILES_arm64       := gcam/lib64/libgcam.so
LOCAL_PROPRIETARY_MODULE    := true
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/gcam/include
LOCAL_MULTILIB := both
include $(BUILD_PREBUILT)
