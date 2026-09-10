#!/bin/bash
#
# Copyright (C) 2017-2026 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

set -e

# Default values (can be overridden by arguments)
DEVICE_COMMON=${1:-???}
PROPRIETARY_FILES=${2:-???}
NO_CLEANUP=false
SECTION=

shift 2
while [ "${#}" -gt 0 ]; do
    case "${1}" in
        -n | --no-cleanup )
            NO_CLEANUP=true
            ;;
        -s | --section )
            SECTION="${2}"
            shift
            NO_CLEANUP=true
            ;;
    esac
    shift
done

if [ "$DEVICE_COMMON" == "universal7870-common" ];then
DEVICE_COMMON=universal7870-common
VENDOR=samsung
else
# fix stuff
VENDOR="samsung/$(dirname "${DEVICE_COMMON}")"
DEVICE_COMMON="$(basename "${DEVICE_COMMON}")"
fi


OUTDIR=

export INITIAL_COPYRIGHT_YEAR=2026

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

# Create universal7870-common-vendor.mk file
VENDOR_DEVICE_MAKEFILE="${ANDROID_ROOT}/vendor/samsung/universal7870-common/universal7870-common-vendor.mk"

# Create Android.mk file
VENDOR_MAKEFILE="${ANDROID_ROOT}/vendor/samsung/universal7870-common/Android.mk"

# Ensure vendor device-makefile exists
touch "${VENDOR_DEVICE_MAKEFILE}"

# Ensure vendor makefile exists
touch "${VENDOR_MAKEFILE}"

# backup work that vendor setup overrides
if [ "$DEVICE_COMMON" == "universal7870-common" ];then
mv "$VENDOR_DEVICE_MAKEFILE" "$VENDOR_DEVICE_MAKEFILE.temp"
fi

# Initialize the helper
if [ "${NO_CLEANUP}" = true ]; then
    CLEAN_SETUP_VENDOR=false
else
    CLEAN_SETUP_VENDOR=true
fi
setup_vendor "${DEVICE_COMMON}" "${VENDOR}" "${ANDROID_ROOT}" true "${CLEAN_SETUP_VENDOR}"

build_current_specs_file() {
    local current_specs_file
    current_specs_file="$(mktemp)"
    parse_file_list "${MY_DIR}/${PROPRIETARY_FILES}" "${SECTION}"

    : > "${current_specs_file}"

    local i
    local count_copy=${#PRODUCT_COPY_FILES_LIST[@]}
    for (( i=0; i<count_copy; i++ )); do
        print_spec false \
            "$(src_file "${PRODUCT_COPY_FILES_LIST[$i]}")" \
            "$(target_file "${PRODUCT_COPY_FILES_LIST[$i]}")" \
            "$(target_args "${PRODUCT_COPY_FILES_LIST[$i]}")" \
            "${PRODUCT_COPY_FILES_HASHES[$i]}" \
            "${PRODUCT_COPY_FILES_FIXUP_HASHES[$i]}" >> "${current_specs_file}"
    done

    local count_pkg=${#PRODUCT_PACKAGES_LIST[@]}
    for (( i=0; i<count_pkg; i++ )); do
        print_spec true \
            "$(src_file "${PRODUCT_PACKAGES_LIST[$i]}")" \
            "$(target_file "${PRODUCT_PACKAGES_LIST[$i]}")" \
            "$(target_args "${PRODUCT_PACKAGES_LIST[$i]}")" \
            "${PRODUCT_PACKAGES_HASHES[$i]}" \
            "${PRODUCT_PACKAGES_FIXUP_HASHES[$i]}" >> "${current_specs_file}"
    done

    echo "${current_specs_file}"
}

CURRENT_SPECS_FILE="$(build_current_specs_file)"
EFFECTIVE_PROPRIETARY_FILE_TMP=
trap 'rm -f "${CURRENT_SPECS_FILE}" "${EFFECTIVE_PROPRIETARY_FILE_TMP}"' EXIT

EFFECTIVE_PROPRIETARY_FILE="${CURRENT_SPECS_FILE}"
if [ "${NO_CLEANUP}" = true ]; then
    PREVIOUS_MERGED_SPECS_FILE="${ANDROID_ROOT}/vendor/${VENDOR}/${DEVICE_COMMON}/.proprietary-files-merged.txt"
    MERGED_SPECS_FILE="$(mktemp)"

    if [ -f "${PREVIOUS_MERGED_SPECS_FILE}" ]; then
        cat "${PREVIOUS_MERGED_SPECS_FILE}" "${CURRENT_SPECS_FILE}" \
            | sed '/^[[:space:]]*$/d' \
            | LC_ALL=C sort -u > "${MERGED_SPECS_FILE}"
    else
        cat "${CURRENT_SPECS_FILE}" \
            | sed '/^[[:space:]]*$/d' \
            | LC_ALL=C sort -u > "${MERGED_SPECS_FILE}"
    fi

    EFFECTIVE_PROPRIETARY_FILE="${MERGED_SPECS_FILE}"
    EFFECTIVE_PROPRIETARY_FILE_TMP="${MERGED_SPECS_FILE}"
fi

# Warning headers and guards
write_headers "a3y17lte j5y17lte a6lte j6lte j7velte j7xelte j7y17lte on7xelte m10lte j7popelteskt"

# The standard blobs
write_makefiles "${EFFECTIVE_PROPRIETARY_FILE}" true

# Force Android.bp owner to samsung
ANDROID_BP_FILE="${ANDROID_ROOT}/vendor/${VENDOR}/${DEVICE_COMMON}/Android.bp"
if [ -f "${ANDROID_BP_FILE}" ]; then
    sed -i 's|owner: "samsung/universal7870-common"|owner: "samsung"|g' "${ANDROID_BP_FILE}"
fi

append_content() {
    local content="# Create Mali links for Vulkan and OpenCL
PRODUCT_PACKAGES += \\
    libGLES_mali

# common audio
ifeq (\$(TARGET_DEVICE_HAS_SEC_AUDIO_HAL),true)
TARGET_DEVICE_COMMON_SEC_AUDIO_HAL := true
endif

ifeq (\$(TARGET_DEVICE_HAS_TFA_SEC_AUDIO_HAL),true)
TARGET_DEVICE_COMMON_SEC_AUDIO_HAL := true
endif

ifeq (\$(TARGET_DEVICE_COMMON_SEC_AUDIO_HAL),true)
PRODUCT_PACKAGES += \\
    libaudior7870 \\
    libLifevibes_lvverx \\
    libLifevibes_lvvetx \\
    libpreprocessing_nxp \\
    librecordalive \\
    libsamsungDiamondVoice \\
    libSamsungPostProcessConvertor \\
    libsecaudioinfo \\
    lib_soundaliveresampler \\
    lib_SoundAlive_SRC384_ver320 \\
    libalsa7870 \\
    audio.primary.exynos7870
endif

# a6lte audio
ifeq (\$(TARGET_DEVICE_HAS_SEC_AUDIO_HAL),true)
PRODUCT_PACKAGES += \\
    lib_SamsungRec_06004
endif

# m10lte audio
ifeq (\$(TARGET_DEVICE_HAS_TFA_SEC_AUDIO_HAL),true)
PRODUCT_PACKAGES += \\
    lib_SamsungRec_06006
endif

ifeq (\$(TARGET_DEVICE_HAS_TFA_AMP),true)
PRODUCT_PACKAGES += \\
    libtfa98xx
    
PRODUCT_COPY_FILES += \\
    vendor/samsung/universal7870-common/audio/sec_tfa/proprietary/vendor/etc/Tfa9896.cnt:\$(TARGET_COPY_OUT_VENDOR)/etc/Tfa9896.cnt
endif"
    
    printf '%s\n' "$content" >> "${VENDOR_DEVICE_MAKEFILE}"
}

function addcustoms_vendor_makefiles_mk(){
    local content="\
###############################
# CUSTOM PART START EXYNOS7870#
###############################

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
	@echo \"Symlink: vulkan.\$(TARGET_BOARD_PLATFORM).so\"
	@mkdir -p \$@/lib/hw
	@mkdir -p \$@/lib64/hw
	\$(hide) ln -sf ../egl/libGLES_mali.so \$@/lib/hw/vulkan.\$(TARGET_BOARD_PLATFORM).so
	\$(hide) ln -sf ../egl/libGLES_mali.so \$@/lib64/hw/vulkan.\$(TARGET_BOARD_PLATFORM).so
	@echo \"Symlink: libOpenCL.so\"
	\$(hide) ln -sf egl/libGLES_mali.so \$@/lib/libOpenCL.so
	\$(hide) ln -sf egl/libGLES_mali.so \$@/lib64/libOpenCL.so
	@echo \"Symlink: libOpenCL.so.1\"
	\$(hide) ln -sf egl/libGLES_mali.so \$@/lib/libOpenCL.so.1
	\$(hide) ln -sf egl/libGLES_mali.so \$@/lib64/libOpenCL.so.1
	@echo \"Symlink: libOpenCL.so.1.1\"
	\$(hide) ln -sf egl/libGLES_mali.so \$@/lib/libOpenCL.so.1.1
	\$(hide) ln -sf egl/libGLES_mali.so \$@/lib64/libOpenCL.so.1.1

ALL_MODULES.\$(LOCAL_MODULE).INSTALLED := \\
	\$(ALL_MODULES.\$(LOCAL_MODULE).INSTALLED) \$(SYMLINKS)

include \$(BUILD_PREBUILT)

ifeq (\$(TARGET_BOARD_HAS_SEC_AUDIO_HAL),true)
LOCAL_AUDIO_VARIANT_DIR := sec
LOCAL_SAMSUNGREC_VARIANT := 06004
LOCAL_USE_J7DUOLTE_VNDSECRIL := true
LOCAL_EXYNOS7870_AUDIO_GUARD := true
endif

ifeq (\$(TARGET_BOARD_HAS_TFA_SEC_AUDIO_HAL),true)
LOCAL_AUDIO_VARIANT_DIR := sec_tfa
LOCAL_SAMSUNGREC_VARIANT := 06006
LOCAL_USE_J7DUOLTE_VNDSECRIL := true
LOCAL_EXYNOS7870_AUDIO_GUARD := true
endif

# TFA AUDIO shoud be avaiable when needed
ifeq (\$(TARGET_AUDIOHAL_VARIANT),samsung-linaro-exynos7870)
LOCAL_AUDIO_VARIANT_DIR := sec_tfa
LOCAL_USE_J7DUOLTE_VNDSECRIL := true
endif
ifeq (\$(TARGET_AUDIOHAL_VARIANT),samsung-exynos7870)
LOCAL_AUDIO_VARIANT_DIR := sec_tfa
LOCAL_USE_J7DUOLTE_VNDSECRIL := true
endif

include \$(CLEAR_VARS)
LOCAL_MODULE := libaudio-ril
LOCAL_MODULE_OWNER := samsung
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := radio/proprietary/vendor/lib/libaudio-ril.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libvndsecril-client libc++ libc libm libdl
include \$(BUILD_PREBUILT)


include \$(CLEAR_VARS)
LOCAL_MODULE := libvndsecril-client
LOCAL_MODULE_OWNER := samsung
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_64 := radio/proprietary/vendor/lib64/libvndsecril-client.so
LOCAL_SRC_FILES_32 := radio/proprietary/vendor/lib/libvndsecril-client.so
LOCAL_MULTILIB := both
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := liblog libcutils libhardware_legacy libfloatingfeature libc++ libc libm libdl
include \$(BUILD_PREBUILT)

ifeq (\$(LOCAL_EXYNOS7870_AUDIO_GUARD),true)

include \$(CLEAR_VARS)
LOCAL_MODULE := libLifevibes_lvverx
LOCAL_MODULE_OWNER := samsung
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := audio/\$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/libLifevibes_lvverx.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := libcutils libLifevibes_lvvetx libdl libc++ libc libm liblog
LOCAL_ALLOW_UNDEFINED_SYMBOLS := true
# Unresolved symbol: __aeabi_idiv
# Unresolved symbol: __aeabi_idivmod
# Unresolved symbol: __aeabi_ldivmod
# Unresolved symbol: __aeabi_uidiv
# Unresolved symbol: __aeabi_uidivmod
include \$(BUILD_PREBUILT)


include \$(CLEAR_VARS)
LOCAL_MODULE := libLifevibes_lvvetx
LOCAL_MODULE_OWNER := samsung
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := audio/\$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/libLifevibes_lvvetx.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := libcutils libdl libc++ libc libm liblog
LOCAL_ALLOW_UNDEFINED_SYMBOLS := true
# Unresolved symbol: __aeabi_idiv
# Unresolved symbol: __aeabi_idivmod
# Unresolved symbol: __aeabi_ldivmod
# Unresolved symbol: __aeabi_uidiv
# Unresolved symbol: __aeabi_uidivmod
include \$(BUILD_PREBUILT)


include \$(CLEAR_VARS)
LOCAL_MODULE := libpreprocessing_nxp
LOCAL_MODULE_OWNER := samsung
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := audio/\$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/libpreprocessing_nxp.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := liblog libcutils libalsa7870 libaudioutils libexpat libhardware libLifevibes_lvvetx libLifevibes_lvverx libc++ libc libm libdl
include \$(BUILD_PREBUILT)


include \$(CLEAR_VARS)
LOCAL_MODULE := librecordalive
LOCAL_MODULE_OWNER := samsung
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := audio/\$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/librecordalive.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
ifeq (\$(TARGET_BOARD_HAS_SEC_AUDIO_HAL),true)
LOCAL_SHARED_LIBRARIES := liblog libutils libcutils lib_SamsungRec_06004 libsecaudioinfo libc++ libc libm libdl
endif
ifeq (\$(TARGET_BOARD_HAS_TFA_SEC_AUDIO_HAL),true)
LOCAL_SHARED_LIBRARIES := liblog libutils libcutils lib_SamsungRec_06006 libsecaudioinfo libc++ libc libm libdl
endif
include \$(BUILD_PREBUILT)


include \$(CLEAR_VARS)
LOCAL_MODULE := libsamsungDiamondVoice
LOCAL_MODULE_OWNER := samsung
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := audio/\$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/libsamsungDiamondVoice.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libsecaudioinfo libc++ libc libm libdl
include \$(BUILD_PREBUILT)


include \$(CLEAR_VARS)
LOCAL_MODULE := libSamsungPostProcessConvertor
LOCAL_MODULE_OWNER := samsung
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := audio/\$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/libSamsungPostProcessConvertor.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := lib_soundaliveresampler libc++ libc libcutils libdl liblog libm libutils
include \$(BUILD_PREBUILT)


include \$(CLEAR_VARS)
LOCAL_MODULE := lib_SamsungRec_\$(LOCAL_SAMSUNGREC_VARIANT)
LOCAL_MODULE_OWNER := samsung
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := audio/\$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/lib_SamsungRec_\$(LOCAL_SAMSUNGREC_VARIANT).so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
ifeq (\$(TARGET_BOARD_HAS_SEC_AUDIO_HAL),true)
LOCAL_SHARED_LIBRARIES := libc libm libdl liblog libstdc++
endif
ifeq (\$(TARGET_BOARD_HAS_TFA_SEC_AUDIO_HAL),true)
LOCAL_SHARED_LIBRARIES := libc libm libdl liblog
LOCAL_ALLOW_UNDEFINED_SYMBOLS := true
# Unresolved symbol: __aeabi_f2lz
# Unresolved symbol: __aeabi_idiv
# Unresolved symbol: __aeabi_l2f
# Unresolved symbol: __aeabi_ldivmod
endif
include \$(BUILD_PREBUILT)


include \$(CLEAR_VARS)
LOCAL_MODULE := libsecaudioinfo
LOCAL_MODULE_OWNER := samsung
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := audio/\$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/libsecaudioinfo.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := libcutils libutils libfloatingfeature libsecnativefeature libbinder liblog libc++ libc libm libdl
include \$(BUILD_PREBUILT)


include \$(CLEAR_VARS)
LOCAL_MODULE := lib_soundaliveresampler
LOCAL_MODULE_OWNER := samsung
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := audio/\$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/lib_soundaliveresampler.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := libutils lib_SoundAlive_SRC384_ver320 libaudioutils libcutils liblog libc++ libc libm libdl
include \$(BUILD_PREBUILT)


include \$(CLEAR_VARS)
LOCAL_MODULE := lib_SoundAlive_SRC384_ver320
LOCAL_MODULE_OWNER := samsung
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := audio/\$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/lib_SoundAlive_SRC384_ver320.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := libc libdl liblog libm
include \$(BUILD_PREBUILT)

include \$(CLEAR_VARS)
LOCAL_MODULE := libtfa98xx
LOCAL_MODULE_OWNER := samsung
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := audio/\$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/libtfa98xx.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := libcutils libutils liblog libc++ libc libm libdl
include \$(BUILD_PREBUILT)

include \$(CLEAR_VARS)
LOCAL_MODULE := audio.primary.exynos7870
LOCAL_MODULE_OWNER := samsung
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := audio/\$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/hw/audio.primary.exynos7870.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
ifeq (\$(TARGET_BOARD_HAS_SEC_AUDIO_HAL),true)
LOCAL_SHARED_LIBRARIES := libSamsungPostProcessConvertor libaudio-ril libaudior7870 libaudioroute_sec_helper libaudioutils libc++ libc libcutils libdl libfloatingfeature liblog libm libpreprocessing_nxp librecordalive libsamsungDiamondVoice libsecaudioinfo libalsa7870 libtinycompress libutils libvndsecril-client
endif
ifeq (\$(TARGET_BOARD_HAS_TFA_SEC_AUDIO_HAL),true)
LOCAL_SHARED_LIBRARIES := libSamsungPostProcessConvertor libalsa7870 libaudio-ril libaudior7870 libaudioroute_sec_helper libaudioutils libc++ libc libcutils libdl libfloatingfeature liblog libm libpreprocessing_nxp librecordalive libsamsungDiamondVoice libsecaudioinfo libtfa98xx libtinycompress libutils libvndsecril-client
endif
include \$(BUILD_PREBUILT)
endif

"

    printf '%s\n' "$content" >> "${VENDOR_MAKEFILE}"

append_content

}

# Process the case
case "${VENDOR}" in     

    samsung)
    case "${DEVICE_COMMON}" in
            universal7870-common)
            VENDOR_DEVICE_MAKEFILE_CASE="${ANDROID_ROOT}/vendor/${VENDOR}/${DEVICE_COMMON}/${DEVICE_COMMON}-vendor.mk"
            addcustoms_vendor_makefiles_mk
                ;;
    esac
    ;;

    samsung/universal7870-common/camera)
        case "${DEVICE_COMMON}" in
            Q)
                VENDOR_DEVICE_MAKEFILE_CASE="${ANDROID_ROOT}/vendor/${VENDOR}/${DEVICE_COMMON}/${DEVICE_COMMON}-vendor.mk"
                
                if ! grep -q "# Camera" "${VENDOR_DEVICE_MAKEFILE}"; then
                    echo "# Camera" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "-include vendor/samsung/universal7870-common/camera/Q/Q-vendor.mk" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "Added Camera to makefile"
                else
                    echo "Camera already in makefile"
                fi
                ;;

        esac
        ;;
        
    samsung/universal7870-common)
        case "${DEVICE_COMMON}" in
            secapp)
                VENDOR_DEVICE_MAKEFILE_CASE="${ANDROID_ROOT}/vendor/${VENDOR}/${DEVICE_COMMON}/${DEVICE_COMMON}-vendor.mk"
                if ! grep -q "# Secapp" "${VENDOR_DEVICE_MAKEFILE}" ; then
                    echo "# Secapp" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "-include vendor/samsung/universal7870-common/secapp/secapp-vendor.mk" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "Added Secapp to makefile"
                else
                    echo "Secapp already in makefile"
                fi
                ;;
            sensors)
                VENDOR_DEVICE_MAKEFILE_CASE="${ANDROID_ROOT}/vendor/${VENDOR}/${DEVICE_COMMON}/${DEVICE_COMMON}-vendor.mk"
                if ! grep -q "# Sensors" "${VENDOR_DEVICE_MAKEFILE}" ; then
                    echo "# Sensors" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "-include vendor/samsung/universal7870-common/sensors/sensors-vendor.mk" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "Added Sensors to makefile"
                else
                    echo "Sensors already in makefile"
                fi
                ;;
            drm)
                VENDOR_DEVICE_MAKEFILE_CASE="${ANDROID_ROOT}/vendor/${VENDOR}/${DEVICE_COMMON}/${DEVICE_COMMON}-vendor.mk"
                if ! grep -q "# DRM" "${VENDOR_DEVICE_MAKEFILE}" ; then
                    echo "# DRM" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "-include vendor/samsung/universal7870-common/drm/drm-vendor.mk" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "Added DRM to makefile"
                else
                    echo "DRM already in makefile"
                fi
                ;;
            media)
                VENDOR_DEVICE_MAKEFILE_CASE="${ANDROID_ROOT}/vendor/${VENDOR}/${DEVICE_COMMON}/${DEVICE_COMMON}-vendor.mk"
                if ! grep -q "# Media" "${VENDOR_DEVICE_MAKEFILE}" ; then
                    echo "# Media" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "-include vendor/samsung/universal7870-common/media/media-vendor.mk" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "Added Media to makefile"
                else
                    echo "Media already in makefile"
                fi
                ;;
            gnss)
                VENDOR_DEVICE_MAKEFILE_CASE="${ANDROID_ROOT}/vendor/${VENDOR}/${DEVICE_COMMON}/${DEVICE_COMMON}-vendor.mk"
                if ! grep -q "# GNSS" "${VENDOR_DEVICE_MAKEFILE}" ; then
                    echo "# GNSS" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "ifeq (\$(TARGET_DEVICE_HAS_SEC_GNSS),true)" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "-include vendor/samsung/universal7870-common/gnss/gnss-vendor.mk" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "endif" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "Added GNSS to makefile"
                else
                    echo "GNSS already in makefile"
                fi
                ;;
            samsung-slsi)
                VENDOR_DEVICE_MAKEFILE_CASE="${ANDROID_ROOT}/vendor/${VENDOR}/${DEVICE_COMMON}/${DEVICE_COMMON}-vendor.mk"
                if ! grep -q "# Samsung SLSI" "${VENDOR_DEVICE_MAKEFILE}" ; then
                    echo "# Samsung SLSI" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "ifeq (\$(TARGET_DEVICE_HAS_SAMSUNG_SLSI_EXYNOS7870),true)" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "-include vendor/samsung/universal7870-common/samsung-slsi/samsung-slsi-vendor.mk" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "endif" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "Added Samsung SLSI to makefile"
                else
                    echo "Samsung SLSI already in makefile"
                fi
                ;;
            radio)
                VENDOR_DEVICE_MAKEFILE_CASE="${ANDROID_ROOT}/vendor/${VENDOR}/${DEVICE_COMMON}/${DEVICE_COMMON}-vendor.mk"
                # those shoud not be copyied, they are handeld by modules
                sed -i -E '/libvndsecril-client\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/libaudio-ril\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"

                if ! grep -q "# Radio" "${VENDOR_DEVICE_MAKEFILE}" ; then
                    echo "# Radio" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "ifeq (\$(TARGET_DEVICE_HAS_SEC_RIL),true)" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "-include vendor/samsung/universal7870-common/radio/radio-vendor.mk" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "endif" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "Added Radio to makefile"
                else
                    echo "Radio already in makefile"
                fi
                ;;
            gatekeeper)
                VENDOR_DEVICE_MAKEFILE_CASE="${ANDROID_ROOT}/vendor/${VENDOR}/${DEVICE_COMMON}/${DEVICE_COMMON}-vendor.mk"
                if ! grep -q "# Gatekeeper" "${VENDOR_DEVICE_MAKEFILE}" ; then
                    echo "# Gatekeeper" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "ifeq (\$(TARGET_DEVICE_HAS_HW_GATEKEEPER_COMMON),true)" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "-include vendor/samsung/universal7870-common/gatekeeper/gatekeeper-vendor.mk" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "endif" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "Added Gatekeeper to makefile"
                else
                    echo "Gatekeeper already in makefile"
                fi
                ;;
            gatekeeper-biometrics)
                VENDOR_DEVICE_MAKEFILE_CASE="${ANDROID_ROOT}/vendor/${VENDOR}/${DEVICE_COMMON}/${DEVICE_COMMON}-vendor.mk"
                if ! grep -q "# Gatekeeper Biometrics" "${VENDOR_DEVICE_MAKEFILE}" ; then
                    echo "# Gatekeeper Biometrics" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "ifeq (\$(TARGET_DEVICE_HAS_HW_GATEKEEPER_BIOMETRICS),true)" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "-include vendor/samsung/universal7870-common/gatekeeper-biometrics/gatekeeper-biometrics-vendor.mk" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "endif" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "Added Gatekeeper Biometrics to makefile"
                else
                    echo "Gatekeeper Biometrics already in makefile"
                fi
                ;;
            tee)
                VENDOR_DEVICE_MAKEFILE_CASE="${ANDROID_ROOT}/vendor/${VENDOR}/${DEVICE_COMMON}/${DEVICE_COMMON}-vendor.mk"
                if ! grep -q "# Teegris" "${VENDOR_DEVICE_MAKEFILE}" ; then
                    echo "# Teegris" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "-include vendor/samsung/universal7870-common/tee/tee-vendor.mk" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "Added Teegris to makefile"
                else
                    echo "Teegris already in makefile"
                fi
                ;;
        esac
        ;;
        
    samsung/universal7870-common/audio)
        case "${DEVICE_COMMON}" in
            sec)
             
                VENDOR_DEVICE_MAKEFILE_CASE="${ANDROID_ROOT}/vendor/${VENDOR}/${DEVICE_COMMON}/${DEVICE_COMMON}-vendor.mk"
                # those shoud not be copyied, they are handeld by modules
                sed -i -E '/libaudior7870\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/libLifevibes_lvverx\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/libLifevibes_lvvetx\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/libpreprocessing_nxp\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/librecordalive\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/libsamsungDiamondVoice\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/libSamsungPostProcessConvertor\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/audio\.primary\.exynos7870\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/lib_SamsungRec_06004\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/lib_SoundAlive_SRC384_ver320\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/lib_soundaliveresampler\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/libsecaudioinfo\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/libalsa7870\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"

                # fix trailing \
                sed -i 's|mVoIPSec/Tx_ControlParams_WIDEBAND_WIRED_HEADSET.txt \\$|mVoIPSec/Tx_ControlParams_WIDEBAND_WIRED_HEADSET.txt|' "$VENDOR_DEVICE_MAKEFILE_CASE"

                if ! grep -q "# Audio - SEC" "${VENDOR_DEVICE_MAKEFILE}" ; then
                    echo "# Audio - SEC" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "ifeq (\$(TARGET_DEVICE_HAS_SEC_AUDIO),true)" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "-include vendor/samsung/universal7870-common/audio/sec/sec-vendor.mk" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "endif" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "Added Audio SEC to makefile"

                    # cleanup
                else
                    echo "Audio SEC already in makefile"
                fi
                ;;
            sec_tfa)
                VENDOR_DEVICE_MAKEFILE_CASE="${ANDROID_ROOT}/vendor/${VENDOR}/${DEVICE_COMMON}/${DEVICE_COMMON}-vendor.mk"
                # those shoud not be copyied, they are handeld by modules
                sed -i -E '/libaudior7870\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/libLifevibes_lvverx\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/libLifevibes_lvvetx\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/libpreprocessing_nxp\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/librecordalive\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/libsamsungDiamondVoice\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/libSamsungPostProcessConvertor\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/audio\.primary\.exynos7870\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/lib_SamsungRec_06006\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/lib_SoundAlive_SRC384_ver320\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/lib_soundaliveresampler\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/libsecaudioinfo\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/libalsa7870\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/libtfa98xx\.so/d' "$VENDOR_DEVICE_MAKEFILE_CASE"
                sed -i -E '/Tfa9896\.cnt/d' "$VENDOR_DEVICE_MAKEFILE_CASE"

                sed -i 's|mVoIPSec/Tx_ControlParams_WIDEBAND_WIRED_HEADSET.txt \\$|mVoIPSec/Tx_ControlParams_WIDEBAND_WIRED_HEADSET.txt|' "$VENDOR_DEVICE_MAKEFILE_CASE"

                if ! grep -q "# Audio - TFA SEC" "${VENDOR_DEVICE_MAKEFILE}" ; then
                    echo "# Audio - TFA SEC" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "ifeq (\$(TARGET_DEVICE_HAS_TFA_SEC_AUDIO),true)" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "-include vendor/samsung/universal7870-common/audio/sec_tfa/sec_tfa-vendor.mk" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "endif" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "" >> "${VENDOR_DEVICE_MAKEFILE}"
                    echo "Added Audio TFA SEC to makefile"

                else
                    echo "Audio TFA SEC already in makefile"
                fi
                ;;
        esac
        ;;
esac

# restore work
if [ "$DEVICE_COMMON" == "universal7870-common" ];then

# cleanup thing to not copy
sed -i -E '/egl\/libGLES_mali\.so/d' "$VENDOR_DEVICE_MAKEFILE"

cat "$VENDOR_DEVICE_MAKEFILE.temp" >> "$VENDOR_DEVICE_MAKEFILE"
rm "$VENDOR_DEVICE_MAKEFILE.temp"
fi

# Finish
write_footers
