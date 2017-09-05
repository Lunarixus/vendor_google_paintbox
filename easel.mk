#
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
#

# Easel firmware images
PRODUCT_PACKAGES += \
    easel/fip.bin \
    easel/Image \
    easel/mnh.dtb \
    easel_ramdisk.img

# vendor/google_paintbox
PRODUCT_PACKAGES += \
    libhdrplusclientimpl \
    libhdrplusmessenger \
    libeaselcomm \
    libeaselcontrol

# Add addtional utils in userdebug and eng builds
# Also includes modules for ramdisk so they could
# be extracted from target files.
ifneq (,$(filter eng userdebug, $(TARGET_BUILD_VARIANT)))
PRODUCT_PACKAGES += \
    easel_boot_test \
    ezlsh \
    ezlspi \
    libeaselsystem \
    libeaselcontrolservice \
    libgcam \
    libhdrplusservice \
    libimageprocessor \
    libpbticlient \
    libpbtimessenger \
    libpbtiservice \
    logd.easel \
    pbserver \
    pbticlient \
    pbtiserver \
    hdrplus_client_tests
endif
