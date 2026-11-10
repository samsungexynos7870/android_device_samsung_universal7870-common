#
# Copyright (C) 2017 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

# Boot and recovery image rules for Samsung Exynos7870 devices.
#
# Ported into the device tree from hardware/samsung/mkbootimg.mk @ lineage-22.2
# (commit ea4723cbc96a8b12c89d29ce0a913388a8c8b31a), deleted together with the
# dtbhtool/ directory in 7533082756a5. BOARD_CUSTOM_BOOTIMG_MK in
# BoardConfigCommon.mk now points here.
#
# Four changes were needed for lineage-24.0:
#
#   * $(MKBOOTIMG) is no longer defined in core/Makefile; it moved to
#     core/config.mk:725. Both images are built with mkdtbhbootimg from
#     dtbhtool/, which writes boot header v0 with the DTBH block appended and
#     the sha1 id filled in.
#   * Nothing appends --dt to INTERNAL_BOOTIMAGE_ARGS any more. At lineage-22.2
#     core/Makefile:1458-1461 added "--dt $(INSTALLED_DTIMAGE_TARGET)" whenever
#     BOARD_KERNEL_SEPARATED_DT was true; BOARD_KERNEL_SEPARATED_DT is not
#     referenced anywhere in core/Makefile at lineage-24.0, and the mkbootimg
#     fork that understood --dt stops at lineage-23.0. The dt image is passed
#     here instead.
#   * BOOTIMAGE_EXTRA_DEPS and RECOVERYIMAGE_EXTRA_DEPS no longer exist, and
#     INTERNAL_BOOTIMAGE_ARGS no longer carries the mkbootimg version arguments,
#     which mkdtbhbootimg would reject.
#   * The loki_tool detour is gone; it was never used by these devices.
#
# BOARD_KERNEL_SEPARATED_DT and TARGET_CUSTOM_DTBTOOL in BoardConfigCommon.mk
# stay as they are: the latter names the tool used below, and the former no
# longer has any effect in build/make.

LOCAL_PATH := $(call my-dir)

MKDTBHBOOTIMG := $(HOST_OUT_EXECUTABLES)/mkdtbhbootimg$(HOST_EXECUTABLE_SUFFIX)
DTBHTOOL := $(HOST_OUT_EXECUTABLES)/$(TARGET_CUSTOM_DTBTOOL)$(HOST_EXECUTABLE_SUFFIX)

INSTALLED_DTIMAGE_TARGET := $(PRODUCT_OUT)/dt.img

# Most specific paths must come first. Recursive assignment on purpose:
# KERNEL_OUT is only set later, by vendor/lineage/build/tasks/kernel.mk.
possible_dtb_dirs = $(KERNEL_OUT)/arch/$(KERNEL_ARCH)/boot/dts/exynos/ \
    $(KERNEL_OUT)/arch/$(KERNEL_ARCH)/boot/dts/ \
    $(KERNEL_OUT)/arch/$(KERNEL_ARCH)/boot/

define build-dtimage-target
    $(call pretty,"Target dt image: $@")
    $(hide) for dir in $(possible_dtb_dirs); do \
        if [ -d "$$dir" ]; then \
            dtb_dir="$$dir"; \
            break; \
        fi; \
    done; \
    if [ -z "$$dtb_dir" ]; then echo "ERROR: no dtb directory among: $(possible_dtb_dirs)" >&2; exit 1; fi; \
    $(DTBHTOOL) -o $@ -s $(BOARD_KERNEL_PAGESIZE) "$$dtb_dir"
    $(hide) chmod a+r $@
endef

$(INSTALLED_DTIMAGE_TARGET): $(DTBHTOOL) $(INSTALLED_KERNEL_TARGET)
	$(build-dtimage-target)
	@echo "Made DT image: $@"

.PHONY: dtimage
dtimage: $(INSTALLED_DTIMAGE_TARGET)

# Deliberately not $(INTERNAL_BOOTIMAGE_FILES): it is
# $(filter-out --%,$(INTERNAL_BOOTIMAGE_ARGS)), which word-splits the quoted
# --cmdline value, so any cmdline with a space in it turns into two
# prerequisites and Kati dies with "No rule to make target". These devices set no
# INTERNAL_KERNEL_CMDLINE - the cmdline comes from CONFIG_CMDLINE in the defconfig
# - so the hazard is latent today, but the inputs are named explicitly instead.
$(INSTALLED_BOOTIMAGE_TARGET): $(MKDTBHBOOTIMG) $(INSTALLED_RAMDISK_TARGET) $(INSTALLED_DTIMAGE_TARGET) $(INSTALLED_KERNEL_TARGET)
	$(call pretty,"Target boot image: $@")
	$(hide) $(MKDTBHBOOTIMG) \
		--kernel $(call bootimage-to-kernel,$@) \
		$(INTERNAL_BOOTIMAGE_ARGS) \
		--dt $(INSTALLED_DTIMAGE_TARGET) \
		$(BOARD_MKBOOTIMG_ARGS) \
		--output $@
	$(hide) echo -n "SEANDROIDENFORCE" >> $@
	$(hide) $(call assert-max-image-size,$@,$(BOARD_BOOTIMAGE_PARTITION_SIZE))
	@echo "Made boot image: $@"

# core/Makefile appends "--kernel $(recovery_kernel)" to
# INTERNAL_RECOVERYIMAGE_ARGS when BOARD_CUSTOM_BOOTIMG_MK is set.
$(INSTALLED_RECOVERYIMAGE_TARGET): $(MKDTBHBOOTIMG) $(recovery_ramdisk) $(recovery_kernel) $(INSTALLED_DTIMAGE_TARGET)
	$(call pretty,"Target recovery image: $@")
	$(hide) $(MKDTBHBOOTIMG) \
		$(INTERNAL_RECOVERYIMAGE_ARGS) \
		--dt $(INSTALLED_DTIMAGE_TARGET) \
		$(BOARD_MKBOOTIMG_ARGS) \
		--output $@
	$(hide) echo -n "SEANDROIDENFORCE" >> $@
	$(hide) $(call assert-max-image-size,$@,$(BOARD_RECOVERYIMAGE_PARTITION_SIZE))
	@echo "Made recovery image: $@"
