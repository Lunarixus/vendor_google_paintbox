# Copyright 2016 The Android Open Source Project

ifeq ($(TARGET_USES_PAINTBOX), true)
  include $(call all-makefiles-under,$(call my-dir))
endif
