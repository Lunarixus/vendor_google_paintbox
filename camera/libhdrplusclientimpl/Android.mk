# Copyright 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH:= $(call my-dir)

#
# libhdrplusclient
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES :=  \
    ApEaselMetadataManager.cpp \
    EaselManagerClientImpl.cpp \
    HdrPlusClientImpl.cpp

LOCAL_SHARED_LIBRARIES := \
    libcamera_metadata \
    libcutils \
    libhdrplusmessenger \
    liblog \
    libutils \
    libeaselcontrol \
    libeaselcomm

LOCAL_STATIC_LIBRARIES := \
    android.hardware.camera.common@1.0-helper

LOCAL_EXPORT_SHARED_LIBRARY_HEADERS += libeaselcontrol
LOCAL_EXPORT_STATIC_LIBRARY_HEADERS += android.hardware.camera.common@1.0-helper

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/ \
    $(LOCAL_PATH)/include \
    hardware/libhardware/include \
    hardware/google/easel/camera/include \
    hardware/google/easel/camera/libhdrplusclient/include \
    hardware/qcom/camera/msm8998/QCamera2/HAL3 \
    system/core/include \
    system/media/camera/include \
    vendor/google_paintbox/camera/include

LOCAL_EXPORT_C_INCLUDE_DIRS += \
    $(LOCAL_PATH)/include

LOCAL_CFLAGS += -Wall -Wextra -Werror

LOCAL_MODULE:= libhdrplusclientimpl
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := google

include $(BUILD_SHARED_LIBRARY)
