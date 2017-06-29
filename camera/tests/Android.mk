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
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    HdrPlusClientTests.cpp \
    HdrPlusTestBurstInput.cpp \
    HdrPlusTestUtils.cpp

LOCAL_SHARED_LIBRARIES := \
    libcamera_metadata \
    libcutils \
    libdng_sdk \
    libhdrplusclient \
    liblog

LOCAL_STATIC_LIBRARIES := \
    android.hardware.camera.common@1.0-helper

LOCAL_C_INCLUDES += \
    system/media/camera/include

LOCAL_CFLAGS += -Wall -Wextra -Werror

# This is needed to ignore unused parameter warning in libdng_sdk headers.
LOCAL_CFLAGS += -Wno-unused-parameter

LOCAL_MODULE:= hdrplus_client_tests
LOCAL_MODULE_OWNER := google
LOCAL_MODULE_TAGS := tests

include $(BUILD_NATIVE_TEST)
