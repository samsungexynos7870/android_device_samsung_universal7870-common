#
# Copyright (C) 2019 The LineageOS Project
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

LOCAL_PATH := device/samsung/universal7870-common

# Include path
TARGET_SPECIFIC_HEADER_PATH := $(LOCAL_PATH)/include

# Firmware
TARGET_NO_BOOTLOADER := true
TARGET_NO_RADIOIMAGE := true

# Platform
BOARD_VENDOR := samsung
TARGET_BOOTLOADER_BOARD_NAME := exynos7870
TARGET_BOARD_PLATFORM := universal7870
TARGET_SOC := exynos7870
include hardware/samsung_slsi-linaro/config/BoardConfig7870.mk

# Architecture
TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_ABI2 :=
TARGET_CPU_VARIANT := cortex-a53
TARGET_CPU_VARIANT_RUNTIME := cortex-a53

TARGET_2ND_ARCH := arm
TARGET_2ND_ARCH_VARIANT := armv8-a
TARGET_2ND_CPU_ABI := armeabi-v7a
TARGET_2ND_CPU_ABI2 := armeabi
TARGET_2ND_CPU_VARIANT := cortex-a53
TARGET_2ND_CPU_VARIANT_RUNTIME := cortex-a53

# Binder
TARGET_BOARD_SUFFIX := _64
TARGET_USES_64_BIT_BINDER := true
BUILD_BROKEN_DUP_RULES := true
BUILD_BROKEN_PHONY_TARGETS := true

# Extracted with libbootimg
BOARD_CUSTOM_BOOTIMG := true
BOARD_CUSTOM_BOOTIMG_MK := hardware/samsung/mkbootimg.mk
BOARD_MKBOOTIMG_ARGS := --kernel_offset 0x00008000 --ramdisk_offset 0x01000000 --tags_offset 0x00000100
BOARD_KERNEL_CMDLINE := androidboot.wificountrycode=00
BOARD_KERNEL_BASE := 0x10000000
BOARD_KERNEL_PAGESIZE := 2048
BOARD_KERNEL_IMAGE_NAME := Image
BOARD_KERNEL_SEPARATED_DT := true
TARGET_CUSTOM_DTBTOOL := dtbhtoolExynos

TARGET_KERNEL_ADDITIONAL_FLAGS := \
    HOSTCFLAGS="-fuse-ld=lld -Wno-unused-command-line-argument"

# Kernel
TARGET_KERNEL_ARCH := arm64
TARGET_KERNEL_HEADER_ARCH := arm64
TARGET_LINUX_KERNEL_VERSION := 3.18

# Kernel config
TARGET_KERNEL_SOURCE := kernel/samsung/exynos7870

# HIDL
DEVICE_MATRIX_FILE := $(LOCAL_PATH)/configs/compatibility_matrix.xml
PRODUCT_ENFORCE_VINTF_MANIFEST_OVERRIDE := true

# Use these flags if the board has a ext4 partition larger than 2gb
BOARD_HAS_LARGE_FILESYSTEM := true
TARGET_USERIMAGES_USE_EXT4 := true
TARGET_USERIMAGES_USE_F2FS := true
TARGET_FS_CONFIG_GEN := $(LOCAL_PATH)/config.fs

# Partitions
BOARD_BOOTIMAGE_PARTITION_SIZE := 33554432
BOARD_CACHEIMAGE_PARTITION_SIZE := 209715200
BOARD_CACHEIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 39845888
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 2380000000
BOARD_USERDATAIMAGE_PARTITION_SIZE := 54618209280
BOARD_VENDORIMAGE_PARTITION_SIZE := 159744000
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_FLASH_BLOCK_SIZE := 4096
BOARD_ROOT_EXTRA_FOLDERS := efs cpefs

# Vendor separation
TARGET_COPY_OUT_VENDOR := vendor

# APEX
DEXPREOPT_GENERATE_APEX_IMAGE := true

# Audio
USE_XML_AUDIO_POLICY_CONF := 1
AUDIOSERVER_MULTILIB := 32

# Backlight
BACKLIGHT_PATH := "/sys/class/backlight/panel/brightness"

# Camera
BOARD_USE_SAMSUNG_CAMERAFORMAT_NV21 := true
TARGET_USES_MEDIA_EXTENSIONS := true
USE_DEVICE_SPECIFIC_CAMERA := true
BOARD_BACK_CAMERA_SENSOR := 0
BOARD_FRONT_CAMERA_SENSOR := 1

# DRM
TARGET_ENABLE_MEDIADRM_64 := true

# LIBHWJPEG
TARGET_USES_EXYNOS7870_LIBHWJPEG := true

# DT2W
TARGET_TAP_TO_WAKE_NODE := /sys/class/sec/tsp/dt2w_enable

# HALs
TARGET_POWERHAL_VARIANT := samsung

# Libhwui
HWUI_COMPILE_FOR_PERF := true

#Offline charge
BOARD_CHARGING_MODE_BOOTING_LPM := "/sys/class/power_supply/battery/batt_lp_charging"
BOARD_CHARGER_SHOW_PERCENTAGE := true
CHARGING_ENABLED_PATH := "/sys/class/power_supply/battery/batt_lp_charging"

# Recovery
BOARD_HAS_DOWNLOAD_MODE := true
TARGET_RECOVERY_FSTAB := $(LOCAL_PATH)/rootdir/etc/fstab.recovery

# RIL
BOARD_VENDOR := samsung
BOARD_MODEM_TYPE := tss310
BOARD_PROVIDES_LIBRIL := true
ENABLE_VENDOR_RIL_SERVICE := true

# Security patch level
VENDOR_SECURITY_PATCH := 2020-12-05

# CURL
BOARD_USES_CURL := true

# Renderscript
OVERRIDE_RS_DRIVER := libRSDriverArm.so

# Seccomp
BOARD_SECCOMP_POLICY := $(LOCAL_PATH)/seccomp

# SELinux
BOARD_SEPOLICY_DIRS := $(LOCAL_PATH)/sepolicy
BOARD_SEPOLICY_VERS := $(PLATFORM_SDK_VERSION).0
SELINUX_IGNORE_NEVERALLOWS := true

# Treble
BOARD_PROPERTY_OVERRIDES_SPLIT_ENABLED := true
PRODUCT_FULL_TREBLE_OVERRIDE := true
PRODUCT_VENDOR_MOVE_ENABLED := true
BOARD_VNDK_VERSION := current
BOARD_USES_VENDORIMAGE := true

# Shim
TARGET_LD_SHIM_LIBS += \
    /system/bin/mediaserver|/system/lib/libstagefright_shim.so

# Build fingerprint
BUILD_FINGERPRINT := "google/coral/coral:13/TPB4.220624.005/8812298:user/release-keys"

# Wifi
BOARD_HAVE_SAMSUNG_WIFI := true
