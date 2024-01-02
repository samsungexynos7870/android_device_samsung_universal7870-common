#!/bin/bash
#
# Copyright (C) 2017-2021 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

set -e

DEVICE_COMMON=universal7870-common
VENDOR=samsung

export INITIAL_COPYRIGHT_YEAR=2017

# proprietary-files_device.txt
declare -A PROP_FILES=(
    ["proprietary-files_a7y17lte.txt"]=""
    ["proprietary-files_m10lte.txt"]=""
    ["proprietary-files_starlte.txt"]=""
    ["proprietary-files_a3y17lte-lineage-19.txt"]=""
)

# Load extract_utils and do some sanity checks
MY_DIR="${BASH_SOURCE%/*}"
if [[ ! -d "${MY_DIR}" ]]; then MY_DIR="${PWD}"; fi

ANDROID_ROOT="${MY_DIR}/../../.."
HELPER="${ANDROID_ROOT}/tools/extract-utils/extract_utils.sh"

if [ ! -f "${HELPER}" ]; then
    echo "Unable to find helper script at ${HELPER}"
    exit 1
fi
source "${HELPER}"

# Initialize the helper
setup_vendor "${DEVICE_COMMON}" "${VENDOR}" "${ANDROID_ROOT}" true

# Warning headers and guards
write_headers "a3y17lte a5y17lte a6lte j6lte j7velte j7xelte j7y17lte on7xelte"

mkdir -p "${MY_DIR}/vendor-tools/unified_proprietary"

cat ${MY_DIR}/vendor-tools/*.txt > "${MY_DIR}/vendor-tools/unified_proprietary/proprietary-files.txt"

# The standard blobs
write_makefiles "${MY_DIR}/vendor-tools/unified_proprietary/proprietary-files.txt" true

rm -rf "${MY_DIR}/vendor-tools/unified_proprietary"

############################################################################################################
# CUSTOM PART START (Taken from https://github.com/LineageOS/android_device_samsung_universal7580-common)  #
############################################################################################################
OUTDIR=vendor/$VENDOR/$DEVICE_COMMON
(cat << EOF) >> $ANDROID_ROOT/$OUTDIR/Android.mk
include \$(CLEAR_VARS)
LOCAL_MODULE := libGLES_mali
LOCAL_MODULE_OWNER := samsung
LOCAL_SRC_FILES_64 := proprietary/vendor/lib64/egl/libGLES_mali.so
LOCAL_SRC_FILES_32 := proprietary/vendor/lib/egl/libGLES_mali.so
LOCAL_MULTILIB := both
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_CHECK_ELF_FILES := false
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_PATH_32 := \$(\$(TARGET_2ND_ARCH_VAR_PREFIX)TARGET_OUT_VENDOR_SHARED_LIBRARIES)/egl
LOCAL_MODULE_PATH_64 := \$(TARGET_OUT_VENDOR_SHARED_LIBRARIES)/egl

SYMLINKS := \$(TARGET_OUT_VENDOR)
\$(SYMLINKS):
	@echo "Symlink: vulkan.\$(TARGET_BOARD_PLATFORM).so"
	@mkdir -p \$@/lib/hw
	@mkdir -p \$@/lib64/hw
	\$(hide) ln -sf ../egl/libGLES_mali.so \$@/lib/hw/vulkan.\$(TARGET_BOARD_PLATFORM).so
	\$(hide) ln -sf ../egl/libGLES_mali.so \$@/lib64/hw/vulkan.\$(TARGET_BOARD_PLATFORM).so
	@echo "Symlink: libOpenCL.so"
	\$(hide) ln -sf egl/libGLES_mali.so \$@/lib/libOpenCL.so
	\$(hide) ln -sf egl/libGLES_mali.so \$@/lib64/libOpenCL.so
	@echo "Symlink: libOpenCL.so.1"
	\$(hide) ln -sf egl/libGLES_mali.so \$@/lib/libOpenCL.so.1
	\$(hide) ln -sf egl/libGLES_mali.so \$@/lib64/libOpenCL.so.1
	@echo "Symlink: libOpenCL.so.1.1"
	\$(hide) ln -sf egl/libGLES_mali.so \$@/lib/libOpenCL.so.1.1
	\$(hide) ln -sf egl/libGLES_mali.so \$@/lib64/libOpenCL.so.1.1

ALL_MODULES.\$(LOCAL_MODULE).INSTALLED := \\
	\$(ALL_MODULES.\$(LOCAL_MODULE).INSTALLED) \$(SYMLINKS)

include \$(BUILD_PREBUILT)

EOF
(cat << EOF) >> $ANDROID_ROOT/$OUTDIR/$DEVICE_COMMON-vendor.mk

# Create Mali links for Vulkan and OpenCL
PRODUCT_PACKAGES += libGLES_mali
EOF
###################################################################################################
# CUSTOM PART END                                                                                 #
###################################################################################################

# Finish
write_footers
