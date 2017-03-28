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
# libhdrplusmessenger
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES :=  \
    EaselMessenger.cpp \
    MessengerToHdrPlusClient.cpp \
    MessengerListenerFromHdrPlusClient.cpp \
    MessengerToHdrPlusService.cpp \
    MessengerListenerFromHdrPlusService.cpp

LOCAL_SHARED_LIBRARIES:= \
    libutils \
    liblog

ifeq ($(USE_LIB_EASEL), 0)
       LOCAL_STATIC_LIBRARIES := libmockeasel
       LOCAL_CFLAGS += -DUSE_LIB_EASEL=0
else
       LOCAL_STATIC_LIBRARIES := libeasel
       LOCAL_CFLAGS += -DUSE_LIB_EASEL=1
endif

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/include \
    hardware/google/paintbox/kernel-headers \
    vendor/google_paintbox/camera/include \
    vendor/google_paintbox/libeasel/include

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include

LOCAL_CFLAGS += -Wall -Wextra -Werror

LOCAL_MODULE:= libhdrplusmessenger
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)