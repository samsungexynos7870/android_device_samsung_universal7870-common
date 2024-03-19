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

# Load extract_utils and do some sanity checks
MY_DIR="${BASH_SOURCE%/*}"
if [[ ! -d "${MY_DIR}" ]]; then MY_DIR="${PWD}"; fi

OUTDIR=vendor/$VENDOR/$DEVICE_COMMON

ANDROID_ROOT="${MY_DIR}/../../.."
HELPER="${ANDROID_ROOT}/tools/extract-utils/extract_utils.sh"

VENDOR_MK_ROOT="${ANDROID_ROOT}"/vendor/"${VENDOR}"/"${DEVICE_COMMON}"

# BLOB_ROOT
BLOB_ROOT="${ANDROID_ROOT}"/vendor/"${VENDOR}"/"${DEVICE_COMMON}"/proprietary

if [ ! -f "${HELPER}" ]; then
    echo "Unable to find helper script at ${HELPER}"
    exit 1
fi

generate_prop_files_array() {
# The path to vendor-tools directory
    local vendor_tools_dir="$1"
# Declare PROP_FILES as a global associative array    
    declare -gA PROP_FILES

# List all 'proprietary-files_*.txt' files in the vendor-tools directory
    local files=(${vendor_tools_dir}/proprietary-files_*.txt)
    for file_path in "${files[@]}"; do
        if [[ -f "$file_path" ]]; then
            local filename=$(basename "$file_path")
# Add to associative array with empty value            
            PROP_FILES["$filename"]=""
        fi
    done
}

generate_prop_files_array "${MY_DIR}/vendor-tools"

# common helper
source "${HELPER}"

# Initialize the helper
setup_vendor "${DEVICE_COMMON}" "${VENDOR}" "${ANDROID_ROOT}" true

# Warning headers and guards
write_headers "a3y17lte a5y17lte a6lte j6lte j7velte j7xelte j7y17lte on7xelte"

mkdir -p "${MY_DIR}/vendor-tools/unified_proprietary"
mkdir -p "${MY_DIR}/vendor-tools/ununified_proprietary_no_guards"
mkdir -p "${MY_DIR}/vendor-tools/ununified_proprietary_guards"

fixup_product_copy_files() {
    local condition="$1"
    local extra_folder="$2"
    local file="$3"
    local block_start="ifeq (\$(${condition}),true)"
    local block_end="endif # ${condition}"
    local in_block=false
    local only_once=false
    
    # Create a temporary file
    local temp_file=$(mktemp)

    # Read through the original file and copy content to the temp file, excluding the old block
    while IFS= read -r line || [[ -n "$line" ]]; do
        if [[ "$line" == "$block_start" ]]; then
            # Start of block to modify, skip copying this line and everything until 'endif'
            in_block=true
            # Also, start capturing the new condition block to insert later
            echo "$line" >> "$temp_file"
            continue
        elif [[ "$line" == "$block_end" ]] && [[ "$in_block" == true ]]; then
            # End of block, skip copying and reset the in_block flag
            echo "$line" >> "$temp_file"
            in_block=false
            continue
        fi
        
        # If not inside the block, or after the block ended, copy the line as is
        if ! $in_block; then
            echo "$line" >> "$temp_file"
        elif [[ $in_block ]] && [[ "$line" =~ ^[[:space:]]*vendor/samsung/ ]]; then
            # Lines within the block that need to be modified
            line=${line/vendor\/samsung\/universal7870-common\/proprietary/vendor\/samsung\/universal7870-common\/proprietary\/$extra_folder}
            
            if ! $only_once ; then
            	only_once=true
            	echo 'PRODUCT_COPY_FILES += \' >> "$temp_file"
            	continue
            fi
            echo "$line" >> "$temp_file"
        fi
    done < "$file"
    
    echo 'endif' >> "$temp_file"
    
    # Overwrite the original file with the new temp file
    mv "$temp_file" "$file"

    echo "Fixup complete for $condition block."
}

# create_local_module() {
# (cat << EOF) >> $ANDROID_ROOT/$OUTDIR/Android.mk
# include \$(CLEAR_VARS)
# LOCAL_MODULE := $MY_LOCAL_MODULE
# LOCAL_MODULE_OWNER := $VENDOR
# LOCAL_VENDOR_MODULE := true
# LOCAL_SRC_FILES_32 := $MY_LOCAL_TYPE/\$(LOCAL_AUDIO_VARIANT_DIR)/$MY_LOCAL_PATH/$MY_LOCAL_FILE
# LOCAL_MODULE_TAGS := optional
# LOCAL_MODULE_SUFFIX := .so
# LOCAL_MULTILIB := 32
# LOCAL_MODULE_CLASS := SHARED_LIBRARIES
# LOCAL_SHARED_LIBRARIES := $MY_LOCAL_SHARED_LIBRARIES
# include \$(BUILD_PREBUILT)
# endif
# EOF
#
#}


create_local_shared_libraries() {
    # Ensure there's an argument provided
    if [[ -z "$1" ]]; then
        echo "Error: No ELF file provided."
        return 1
    fi
    
    # The ELF file to inspect
    local elf_file="$1"

    # Check that the ELF file exists
    if [[ ! -f "$elf_file" ]]; then
        echo "Error: The specified ELF file does not exist: $elf_file"
        return 1
    fi
    
    # Use patchelf to get a list of NEEDED entries (shared libraries)
    local libraries=$(patchelf --print-needed "$elf_file" | sed 's/\.so$//')

    # Define the LOCAL_SHARED_LIBRARIES variable
    echo "LOCAL_SHARED_LIBRARIES := \\"
    # Iterate over each library and add it to the variable definition
    local delim=""
    for library in $libraries; do
        echo -n "$delim$library"
        delim=" \\\n    " # Set delimiter for subsequent lines
    done
    echo # Append final newline
}


cleanup_product_copy_files() {
    # Input
    if [[ -z "$1" ]]; then
        echo "Error: No input file provided."
        return 1
    fi

    # Neverallow product copy files
    local libraries=(
        libaudior7870
        libLifevibes_lvverx
        libLifevibes_lvvetx
        libpreprocessing_nxp
        librecordalive
        libtfa98xx
        libsamsungDiamondVoice
        libSamsungPostProcessConvertor
        lib_SamsungRec_06004
        lib_SamsungRec_06006
        libsecaudioinfo
        lib_soundaliveresampler
        lib_SoundAlive_SRC384_ver320
        libalsa7870
        audio.primary.exynos7870
        libGLES_mali
        Tfa9896.cnt
        libvndsecril-client
        RootPA.apk
    )

    # The file to cleanup
    local input_mk="$1"
    local output_mk="${input_mk}.clean"

    # Ensure the file exists
    if [[ ! -f "$input_mk" ]]; then
        echo "Error: The file $input_mk does not exist."
        return 1
    fi

    # Read each line of the file and remove lines containing the specified libraries
    while IFS= read -r line; do
        local write_line="true"
        for lib in "${libraries[@]}"; do
            if [[ "$line" == *"$lib"* ]]; then
                write_line="false"
                break
            fi
        done
        if [[ "$write_line" == "true" ]]; then
            echo "$line" >> "$output_mk"
        fi
    done < "$input_mk"

    # Replace the input file with output
    mv "$output_mk" "$input_mk"

    echo "Cleanup product copy files on $input_mk completed."
}

fixup_endif() {
    # Input validation
    if [[ -z "$1" ]]; then
        echo "Error: No input file provided."
        return 1
    fi

    # The file to fix up
    local input_mk="$1"
    local output_mk="${input_mk}.tmp"

    # Check the file existence
    if [[ ! -f "$input_mk" ]]; then
        echo "Error: The file $input_mk does not exist."
        return 1
    fi

    # Variable to hold the previous line's content
    local prev_line=""

    # Read each line of the file while preserving white spaces
    while IFS= read -r line || [[ -n "$line" ]]; do
        
        # If the current line is 'endif' and the previous line ends with a '\'
        if [[ "$line" == "endif" ]] && [[ "$prev_line" == *"\\" ]]; then
            # Remove the backslash from the end of the previous line
            prev_line="${prev_line%\\}"
        fi
        
        # If there's a non-empty previous line, write it to the output
        if [[ -n "$prev_line" ]]; then
            printf "%s\n" "$prev_line" >> "$output_mk"
        fi
        
        # Set the previous line to the current line for the next iteration
        prev_line="$line"
    done < "$input_mk"

    # Write the last line if it's not 'endif' (since it wouldn't be followed by another line to trigger removal of '\')
    if [[ -n "$prev_line" && "$prev_line" != "endif" ]]; then
        printf "%s\n" "$prev_line" >> "$output_mk"
    fi

    # Move the temporary output file to overwrite the original input file
    mv "$output_mk" "$input_mk"

    echo "Fixup on $input_mk completed."
}



process_guard_proprietary_files() {
    local input_file="${MY_DIR}/vendor-tools/${PROP_FILE}"
    output_file_guarded="${MY_DIR}/vendor-tools/ununified_proprietary_guards/${PROP_FILE}"
    output_file_no_guard="${MY_DIR}/vendor-tools/ununified_proprietary_no_guards/${PROP_FILE}"

    if [[ ! -f "$input_file" ]]; then
        echo "Input file does not exist: $input_file"
        return 1
    fi

    local guard_found=false
    local write_guarded=false
    local write_no_guard=true

    >> "$output_file_guarded" # Empty the output file in case it exists and has content
    >> "$output_file_no_guard" # Empty the output file in case it exists and has content

    while IFS= read -r line; do
        # Check for start and end of guarded sections
        if [[ "$line" == "# START_GUARD:"* ]]; then
            guard_found=true
            write_guarded=true
            write_no_guard=false
            continue
        elif [[ "$line" == "# END_GUARD:"* ]]; then
            write_guarded=false
            write_no_guard=true
            continue
        fi

        # Write to guarded file if within a guarded section
        if $write_guarded; then
            echo "$line" >> "$output_file_guarded"
        fi

        # Write to no-guard file when outside a guarded section
        if $write_no_guard && ! $write_guarded; then
            echo "$line" >> "$output_file_no_guard"
        fi
    done < "$input_file"
       
    if ! $guard_found; then
        echo "No guard comments found in the file: $input_file. Skipping guarded output."
    fi
}

for PROP_FILE in "${!PROP_FILES[@]}"; do
    SOURCE_DIR=${PROP_FILES[$PROP_FILE]}

    process_guard_proprietary_files
    
done

for PROP_FILE in "${!PROP_FILES[@]}"; do
    SOURCE_DIR=${PROP_FILES[$PROP_FILE]}
    
    # helper needs to be in loop too to always get relauched with correct options
    source "${HELPER}"

    if [[ "${PROP_FILE}" == "proprietary-files_m10lte.txt" ]]; then
       echo '# m10lte audio hal' >> "$VENDOR_MK_ROOT/${DEVICE_COMMON}-vendor.mk"
       echo 'ifeq ($(TARGET_DEVICE_HAS_M10LTE_AUDIO_HAL),true)' >> "$VENDOR_MK_ROOT/${DEVICE_COMMON}-vendor.mk"
    fi
    
    if [[ "${PROP_FILE}" == "proprietary-files_a6lte.txt" ]]; then
       echo '# a6lte audio hal' >> "$VENDOR_MK_ROOT/${DEVICE_COMMON}-vendor.mk"
       echo 'ifeq ($(TARGET_DEVICE_HAS_A6LTE_AUDIO_HAL),true)' >> "$VENDOR_MK_ROOT/${DEVICE_COMMON}-vendor.mk"
    fi

    # The standard blobs
    write_makefiles "${MY_DIR}/vendor-tools/ununified_proprietary_guards/${PROP_FILE}" true
    
    if [[ "${PROP_FILE}" == "proprietary-files_m10lte.txt" ]]; then
        echo 'endif' >> "$VENDOR_MK_ROOT/${DEVICE_COMMON}-vendor.mk"
        echo "" >> "$ANDROID_ROOT/$OUTDIR/${DEVICE_COMMON}-vendor.mk"
        fixup_product_copy_files "TARGET_DEVICE_HAS_M10LTE_AUDIO_HAL" "M10LTE_AUDIO" "${VENDOR_MK_ROOT}/${DEVICE_COMMON}-vendor.mk"
    fi

    if [[ "${PROP_FILE}" == "proprietary-files_a6lte.txt" ]]; then
        echo 'endif' >> "$VENDOR_MK_ROOT/${DEVICE_COMMON}-vendor.mk"
        echo "" >> "$ANDROID_ROOT/$OUTDIR/${DEVICE_COMMON}-vendor.mk"
        fixup_product_copy_files "TARGET_DEVICE_HAS_A6LTE_AUDIO_HAL" "A6LTE_AUDIO" "${VENDOR_MK_ROOT}/${DEVICE_COMMON}-vendor.mk"
    fi

    # leave 1 line space before next one
    echo "" >> "$ANDROID_ROOT/$OUTDIR/${DEVICE_COMMON}-vendor.mk"
    
done

cat ${MY_DIR}/vendor-tools/ununified_proprietary_no_guards/*.txt > "${MY_DIR}/vendor-tools/unified_proprietary/proprietary-files.txt"

# common helper
source "${HELPER}"

# The standard blobs
write_makefiles "${MY_DIR}/vendor-tools/unified_proprietary/proprietary-files.txt" true

cleanup_product_copy_files "${ANDROID_ROOT}/${OUTDIR}/${DEVICE_COMMON}-vendor.mk"
fixup_endif "${ANDROID_ROOT}/${OUTDIR}/${DEVICE_COMMON}-vendor.mk"

rm -rf "${MY_DIR}/vendor-tools/unified_proprietary"
rm -rf "${MY_DIR}/vendor-tools/ununified_proprietary_no_guards"
rm -rf "${MY_DIR}/vendor-tools/ununified_proprietary_guards"


############################################################################################################
# CUSTOM PART START (Taken from https://github.com/LineageOS/android_device_samsung_universal7580-common)  #
############################################################################################################
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


# only for android pie devices
# include \$(CLEAR_VARS)
# LOCAL_MODULE := RootPA
# LOCAL_MODULE_OWNER := samsung
# LOCAL_SRC_FILES := proprietary/vendor/app/RootPA/RootPA.apk
# LOCAL_CERTIFICATE := platform
# LOCAL_MODULE_TAGS := optional
# LOCAL_MODULE_CLASS := APPS
# LOCAL_DEX_PREOPT := false
# LOCAL_MODULE_SUFFIX := .apk
# LOCAL_VENDOR_MODULE := true
# include \$(BUILD_PREBUILT)


ifeq (\$(TARGET_BOARD_HAS_A6LTE_AUDIO_HAL),true)
LOCAL_AUDIO_VARIANT_DIR := A6LTE_AUDIO
LOCAL_SAMSUNGREC_VARIANT := 06004
LOCAL_USE_STARLTE_VNDSECRIL := true
endif

ifeq (\$(TARGET_BOARD_HAS_M10LTE_AUDIO_HAL),true)
LOCAL_AUDIO_VARIANT_DIR := M10LTE_AUDIO
LOCAL_SAMSUNGREC_VARIANT := 06006
LOCAL_USE_STARLTE_VNDSECRIL := true
endif

ifeq (\$(TARGET_BOARD_HAS_TFA_AMP),true)
include \$(CLEAR_VARS)
LOCAL_MODULE := libtfa98xx
LOCAL_MODULE_OWNER := $VENDOR
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := proprietary/\$(LOCAL_AUDIO_VARIANT_DIR)/vendor/lib/libtfa98xx.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := libcutils libutils liblog libc++ libc libm libdl
include \$(BUILD_PREBUILT)
endif


ifeq (\$(LOCAL_USE_STARLTE_VNDSECRIL),true)
include \$(CLEAR_VARS)
LOCAL_MODULE := libvndsecril-client
LOCAL_MODULE_OWNER := $VENDOR
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_64 := proprietary/vendor/lib64/libvndsecril-client.so
LOCAL_SRC_FILES_32 := proprietary/vendor/lib/libvndsecril-client.so
LOCAL_MULTILIB := both
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := liblog libcutils libhardware_legacy libfloatingfeature libc++ libc libm libdl
include \$(BUILD_PREBUILT)
endif


include \$(CLEAR_VARS)
LOCAL_MODULE := libaudior7870
LOCAL_MODULE_OWNER := $VENDOR
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := proprietary/\$(LOCAL_AUDIO_VARIANT_DIR)/vendor/lib/libaudior7870.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libexpat libalsa7870 libc++ libc libm libdl
include \$(BUILD_PREBUILT)


include \$(CLEAR_VARS)
LOCAL_MODULE := libLifevibes_lvverx
LOCAL_MODULE_OWNER := $VENDOR
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := proprietary/\$(LOCAL_AUDIO_VARIANT_DIR)/vendor/lib/libLifevibes_lvverx.so
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
LOCAL_MODULE_OWNER := $VENDOR
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := proprietary/\$(LOCAL_AUDIO_VARIANT_DIR)/vendor/lib/libLifevibes_lvvetx.so
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
LOCAL_MODULE_OWNER := $VENDOR
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := proprietary/\$(LOCAL_AUDIO_VARIANT_DIR)/vendor/lib/libpreprocessing_nxp.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := liblog libcutils libalsa7870 libaudioutils libexpat libhardware libLifevibes_lvvetx libLifevibes_lvverx libc++ libc libm libdl
include \$(BUILD_PREBUILT)


include \$(CLEAR_VARS)
LOCAL_MODULE := librecordalive
LOCAL_MODULE_OWNER := $VENDOR
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := proprietary/\$(LOCAL_AUDIO_VARIANT_DIR)/vendor/lib/librecordalive.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
ifeq (\$(TARGET_BOARD_HAS_A6LTE_AUDIO_HAL),true)
LOCAL_SHARED_LIBRARIES := liblog libutils libcutils lib_SamsungRec_06004 libsecaudioinfo libc++ libc libm libdl
endif
ifeq (\$(TARGET_BOARD_HAS_M10LTE_AUDIO_HAL),true)
LOCAL_SHARED_LIBRARIES := liblog libutils libcutils lib_SamsungRec_06006 libsecaudioinfo libc++ libc libm libdl
endif
include \$(BUILD_PREBUILT)


include \$(CLEAR_VARS)
LOCAL_MODULE := libsamsungDiamondVoice
LOCAL_MODULE_OWNER := $VENDOR
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := proprietary/\$(LOCAL_AUDIO_VARIANT_DIR)/vendor/lib/libsamsungDiamondVoice.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libsecaudioinfo libc++ libc libm libdl
include \$(BUILD_PREBUILT)


include \$(CLEAR_VARS)
LOCAL_MODULE := libSamsungPostProcessConvertor
LOCAL_MODULE_OWNER := $VENDOR
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := proprietary/\$(LOCAL_AUDIO_VARIANT_DIR)/vendor/lib/libSamsungPostProcessConvertor.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := lib_soundaliveresampler libc++ libc libcutils libdl liblog libm libutils
include \$(BUILD_PREBUILT)


include \$(CLEAR_VARS)
LOCAL_MODULE := lib_SamsungRec_\$(LOCAL_SAMSUNGREC_VARIANT)
LOCAL_MODULE_OWNER := $VENDOR
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := proprietary/\$(LOCAL_AUDIO_VARIANT_DIR)/vendor/lib/lib_SamsungRec_\$(LOCAL_SAMSUNGREC_VARIANT).so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
ifeq (\$(TARGET_BOARD_HAS_A6LTE_AUDIO_HAL),true)
LOCAL_SHARED_LIBRARIES := libc libm libdl liblog libstdc++
endif
ifeq (\$(TARGET_BOARD_HAS_M10LTE_AUDIO_HAL),true)
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
LOCAL_MODULE_OWNER := $VENDOR
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := proprietary/\$(LOCAL_AUDIO_VARIANT_DIR)/vendor/lib/libsecaudioinfo.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := libcutils libutils libfloatingfeature libsecnativefeature libbinder liblog libc++ libc libm libdl
include \$(BUILD_PREBUILT)


include \$(CLEAR_VARS)
LOCAL_MODULE := lib_soundaliveresampler
LOCAL_MODULE_OWNER := $VENDOR
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := proprietary/\$(LOCAL_AUDIO_VARIANT_DIR)/vendor/lib/lib_soundaliveresampler.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := libutils lib_SoundAlive_SRC384_ver320 libaudioutils libcutils liblog libc++ libc libm libdl
include \$(BUILD_PREBUILT)


include \$(CLEAR_VARS)
LOCAL_MODULE := lib_SoundAlive_SRC384_ver320
LOCAL_MODULE_OWNER := $VENDOR
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := proprietary/\$(LOCAL_AUDIO_VARIANT_DIR)/vendor/lib/lib_SoundAlive_SRC384_ver320.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := libc libdl liblog libm
include \$(BUILD_PREBUILT)


include \$(CLEAR_VARS)
LOCAL_MODULE := libalsa7870
LOCAL_MODULE_OWNER := $VENDOR
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := proprietary/\$(LOCAL_AUDIO_VARIANT_DIR)/vendor/lib/libalsa7870.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := libc++ libc libm libdl
include \$(BUILD_PREBUILT)


include \$(CLEAR_VARS)
LOCAL_MODULE := audio.primary.exynos7870
LOCAL_MODULE_OWNER := $VENDOR
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := proprietary/\$(LOCAL_AUDIO_VARIANT_DIR)/vendor/lib/hw/audio.primary.exynos7870.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
ifeq (\$(TARGET_BOARD_HAS_A6LTE_AUDIO_HAL),true)
LOCAL_SHARED_LIBRARIES := libSamsungPostProcessConvertor libaudio-ril libaudior7870 libaudioutils libc++ libc libcutils libdl libfloatingfeature liblog libm libpreprocessing_nxp librecordalive libsamsungDiamondVoice libsecaudioinfo libalsa7870 libtinycompress libutils libvndsecril-client
endif
ifeq (\$(TARGET_BOARD_HAS_M10LTE_AUDIO_HAL),true)
LOCAL_SHARED_LIBRARIES := libSamsungPostProcessConvertor libaudio-ril libaudior7870 libaudioutils libc++ libc libcutils libdl libfloatingfeature liblog libm libpreprocessing_nxp librecordalive libsamsungDiamondVoice libsecaudioinfo libalsa7870 libtinycompress libutils libvndsecril-client libtfa98xx
endif
include \$(BUILD_PREBUILT)

EOF

(cat << EOF) >> $ANDROID_ROOT/$OUTDIR/$DEVICE_COMMON-vendor.mk

# Create Mali links for Vulkan and OpenCL
PRODUCT_PACKAGES += \\
    libGLES_mali

# common audio
ifeq (\$(TARGET_DEVICE_HAS_PREBUILT_AUDIO_HAL),true)
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
ifeq (\$(TARGET_DEVICE_HAS_A6LTE_AUDIO_HAL),true)
PRODUCT_PACKAGES += \\
    lib_SamsungRec_06004
endif

# m10lte audio
ifeq (\$(TARGET_DEVICE_HAS_M10LTE_AUDIO_HAL),true)
PRODUCT_PACKAGES += \\
    lib_SamsungRec_06006
endif

ifeq (\$(TARGET_DEVICE_HAS_TFA_AMP),true)
PRODUCT_COPY_FILES += \\
    vendor/samsung/universal7870-common/proprietary/M10LTE_AUDIO/vendor/etc/Tfa9896.cnt:\$(TARGET_COPY_OUT_VENDOR)/etc/Tfa9896.cnt

PRODUCT_PACKAGES += \\
    libtfa98xx
endif

EOF
###################################################################################################
# CUSTOM PART END                                                                                 #
###################################################################################################

# Finish
write_footers
