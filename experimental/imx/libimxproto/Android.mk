LOCAL_PATH:= $(call my-dir)

# libimxproto
include $(CLEAR_VARS)
LOCAL_MODULE := libimxproto
LOCAL_MODULE_OWNER := google
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_PROTOC_OPTIMIZE_TYPE := lite
LOCAL_SRC_FILES := $(call all-proto-files-under)
include $(BUILD_STATIC_LIBRARY)

