#
# Copyright (C) 2019-2020 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#
LOCAL_PATH := $(call my-dir)
ifeq ($(TARGET_DEVICE),a3y17lte)
  subdir_makefiles=$(call first-makefiles-under,$(LOCAL_PATH))
  $(foreach mk,$(subdir_makefiles),$(info including $(mk) ...)$(eval include $(mk)))

include $(CLEAR_VARS)

GRALLOC_EXYNOS5_SYMLINK := $(PRODUCT_OUT)/system/lib/hw/gralloc.exynos5.so
$(GRALLOC_EXYNOS5_SYMLINK): $(LOCAL_INSTALLED_MODULE)
	@mkdir -p $(dir $@)
	$(hide) ln -sf /vendor/lib/hw/$(notdir $@) $@

ALL_DEFAULT_INSTALLED_MODULES += $(GRALLOC_EXYNOS5_SYMLINK)

HWCOMPOSER_EXYNOS5_SYMLINK := $(PRODUCT_OUT)/system/lib/hw/gralloc.exynos5.so
$(GRALLOC_EXYNOS5_SYMLINK): $(LOCAL_INSTALLED_MODULE)
	@mkdir -p $(dir $@)
	$(hide) ln -sf /vendor/lib/hw/$(notdir $@) $@

ALL_DEFAULT_INSTALLED_MODULES += $(GRALLOC_EXYNOS5_SYMLINK)

endif
