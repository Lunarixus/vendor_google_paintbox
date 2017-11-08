# Copyright 2017 The Android Open Source Project
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
# libimxclient
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES :=  \
    ImxClient.cpp

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/include \
    vendor/google_paintbox/experimental/imx/libimx/include

LOCAL_EXPORT_C_INCLUDE_DIRS += \
    $(LOCAL_PATH)/include \
    vendor/google_paintbox/easel/comm/include \
    vendor/google_paintbox/experimental/imx/libimxcommon/include

LOCAL_SHARED_LIBRARIES := \
    libbase \
    libeaselcomm \
    libprotobuf-cpp-lite \
    liblog
LOCAL_STATIC_LIBRARIES := \
    libimxcommon \
    libimxproto \
    libimxprotoconversions
LOCAL_CFLAGS += \
    -Wall -Wextra -Werror \
    -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS \
    -Wthread-safety

LOCAL_MODULE:= libimxclient
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := google

include $(BUILD_SHARED_LIBRARY)
