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

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_VENDOR_MODULE := true

LOCAL_BURST := 0080_20170616_120819_772
LOCAL_BURST_DIR := $(LOCAL_PATH)/$(LOCAL_BURST)
TARGET_OUT_DIR := $(TARGET_OUT_DATA)/nativetest/hdrplus_client_tests/bursts/$(LOCAL_BURST)

# Copy DNG and metadata files.
.PHONY: hdrplus-bursts
hdrplus-bursts: $(ACP)
	@mkdir -p $(TARGET_OUT_DIR)
	$(ACP) -fp $(LOCAL_BURST_DIR)/*.dng $(TARGET_OUT_DIR)
	$(ACP) -fp $(LOCAL_BURST_DIR)/*.ppm $(TARGET_OUT_DIR)
	$(ACP) -fp $(LOCAL_BURST_DIR)/payload_burst_actual_hal3.txt $(TARGET_OUT_DIR)
	$(ACP) -fp $(LOCAL_BURST_DIR)/static_metadata_hal3.txt $(TARGET_OUT_DIR)
