#!/bin/bash
# # Copyright (C) 2017-2021 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

set -e

DEVICE_COMMON=universal7870-common
VENDOR=samsung
TOOLS_DIR=vendor-tools

# Load extract_utils and do some sanity checks
MY_DIR="${BASH_SOURCE%/*}"
if [[ ! -d "${MY_DIR}" ]]; then MY_DIR="${PWD}"; fi

ANDROID_ROOT="${MY_DIR}/../../.."
HELPER="${ANDROID_ROOT}/tools/extract-utils/extract_utils.sh"

# BLOB_ROOT
BLOB_ROOT="${ANDROID_ROOT}"/vendor/"${VENDOR}"/"${DEVICE_COMMON}"/proprietary

if [ ! -f "${HELPER}" ]; then
    echo "Unable to find helper script at ${HELPER}"
    exit 1
fi
source "${HELPER}"

# Default to sanitizing the vendor folder before extraction
CLEAN_VENDOR=true

KANG=
SECTION=

###################################################################################
# The function 'generate_prop_files_array' takes a path directory as an argument.
# It locates all files within directory that match 'proprietary-files_*.txt'.
# Each of these files is added to a global associative array named 'PROP_FILES'.
# The filename is set as the key and the value is set as an empty string.
#
# PROP_FILES["proprietary-files_a6lte.txt"]=""                                                             
# PROP_FILES["proprietary-files_m10lte.txt"]=""
###################################################################################

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

generate_prop_files_array "${MY_DIR}/${TOOLS_DIR}"

function usage() {
    echo "Usage: $0 [options]"
    echo
    echo "Options:"
    echo "  -pfs | --proprietary-files-list-and-source <proprietary-files_device.txt> <source_dir> , proprietary-files_somedevice.txt file list with its source directory."
    echo "Currenty need <source_dir> for files listed in following .txt:"
    for key in "${!PROP_FILES[@]}"; do
        echo "$key ${PROP_FILES[$key]}"
    done
    echo ""
    echo "  -n   | --no-cleanup                        Do not clean the vendor directory."
    echo "  -k   | --kang                              Kang (rebrand) proprietary files from another device."
    echo "  -s   | --section                           helper "
    echo "  -h   | --help                              Show this help message."
    echo ""
    echo "example Usage: look at ${TOOLS_DIR}\universal7870-common-extract-files-example.sh"
    echo 
    exit 1
}

while [ "${#}" -gt 0 ]; do
    case "${1}" in
        -n|--no-cleanup)
            CLEAN_VENDOR=false
            ;;
        -k|--kang)
            KANG="--kang"
            ;;
        -s|--section)
            SECTION="${2}"; shift
            CLEAN_VENDOR=false
            ;;
        -pfs|--proprietary-files-list-and-source)
            PROP_FILES["$2"]="${3}"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            usage
            ;;
    esac
    shift
done

# Initialize the helper
setup_vendor "${DEVICE_COMMON}" "${VENDOR}" "${ANDROID_ROOT}" true "${CLEAN_VENDOR}"

move_file() {
    local source_file="$1"
    local destination_file="$2"

    if [[ -f "$source_file" ]]; then
        mkdir -p "$(dirname "$destination_file")" || return 1
        if ! mv -vn "$source_file" "$destination_file"; then
            return 1
        else
            echo "Moved $source_file to $destination_file"
        fi
    else
        echo "Warning: '$source_file' does not exist. Skipping..." >&2
    fi
}

extra_folder_section() {
    local input_file="$1"        # The original file to read from
    local output_file="$2"       # The file to write the section to
    local write_flag=false      
    extra_folder_name=""

    # Check if the input file exists
    if [ ! -f "$input_file" ]; then
        echo "The input file does not exist."
        return 1
    fi

    echo "Processing $input_file and extracting sections to $output_file"

    # Process the input file line by line
    while IFS= read -r line || [[ -n "$line" ]]; do
        echo "Reading line: $line"

        # Check for the start of an EXTRA_FOLDER section
        if [[ "$line" =~ ^#[[:space:]]*START_EXTRA_FOLDER:(.+)$ ]]; then
            extra_folder_name="${BASH_REMATCH[1]}"
            echo "Starting to extract section: $extra_folder_name"
            write_flag=true
            > "$output_file" # Empty the output file first
            continue
        # Check for the end of the current EXTRA_FOLDER section
        elif [[ "$line" =~ ^#[[:space:]]*END_EXTRA_FOLDER ]]; then
            if $write_flag; then
                echo "Finished extracting section: $extra_folder_name"
                write_flag=false
                GLOABL_EXTRA_FOLDER_NAME=$extra_folder_name
                extra_folder_name="" # Reset the name for the next section
            fi
        # If we are within the EXTRA_FOLDER section, write the line to the output file
        elif $write_flag; then
            echo "Writing line to $output_file"
            echo "$line" >> "$output_file"
        fi
    done < "$input_file"

    echo "Finished processing $input_file"
}

process_extra_folder_section_lines_fixup() {
    local input_file="$1"
    local temp_file="${input_file}.tmp"  # Temporary file to store changes

    # Clear the temporary file if it already exists
    : > "$temp_file"

    while IFS= read -r line; do
        # Skip empty lines and comments
        [[ -z "$line" || "$line" =~ ^# ]] && continue
        
        # Edit lines to keep only the part after the colon
        # The `sed` pattern here looks for 'something:something_else' and keeps 'something_else'
        # since we already work with vendor and not source vendor/lib64/libril.so:vendor/lib64/libril-samsung.so
        # only keep vendor/lib64/libril-samsung.so for example
        processed_line=$(echo "$line" | sed -E 's/[^ :]+:([^ ]+)/\1/g')

        # Write the processed line to the temporary file
        echo "$processed_line" >> "$temp_file"
    done < "$input_file"

    # Move the temporary file to replace the original input file
    mv "$temp_file" "$input_file"
}

process_extra_folder_section_lines() {
    local input_file="$1"
    local extra_folder="$2"
    local blob_root="$3"

    local line src dst source_path destination_path
    
    process_extra_folder_section_lines_fixup "$input_file"

    while IFS= read -r line; do
        # Skip empty lines and comments
        [[ -z "$line" || "$line" =~ ^# ]] && continue

        # Assign processed or unprocessed line to src and dst
        src="$line"
        dst="$line"

        source_path="${blob_root}/${src}"
        destination_path="${blob_root}/${extra_folder}/${dst}"

        # Call your existing move_file function with the paths
        move_file "$source_path" "$destination_path"
    done < "$input_file"
}


process_extra_folders() {
    # prop_file equal to proprietary-files_device.txt
    local prop_file="$1"
    # proprietary-files_device.txt is input_file
    local input_file="${MY_DIR}/${TOOLS_DIR}/${prop_file}"

    # output folder for extra_folder_proprietary-files_device.txt
    local extra_folder_section_out="${MY_DIR}/${TOOLS_DIR}/extra_folder_section_out"
    # actual output for extra_folder_proprietary-files_device.txt
    local output_file="${extra_folder_section_out}/${prop_file}"
    
    mkdir -p "${extra_folder_section_out}"
  
    # emty stuff
    : > "$output_file"

    # Call extra_folder_section function to extract the section extraction
    extra_folder_section "$input_file" "$output_file"
    
    local extra_folder="$GLOABL_EXTRA_FOLDER_NAME"

    # Call process_extra_folder_section_lines function to process the section lines
    process_extra_folder_section_lines "$output_file" "$extra_folder" "$BLOB_ROOT"

    # Clean up out
    rm -rf "$extra_folder_section_out"
}


for PROP_FILE in "${!PROP_FILES[@]}"; do
    SOURCE_DIR=${PROP_FILES[$PROP_FILE]}
    # Check if any source directory with proprietary-files is empty
    if [ -z "${SOURCE_DIR}" ]; then
        echo "Error: Source directory not specified for ${PROP_FILE}. Provide it with --file ${PROP_FILE} <source_dir>"
        usage
    fi
    
    # Check if provided source directory exists
    if [ ! -d "${SOURCE_DIR}" ]; then
        echo "Error: Source directory ${SOURCE_DIR} does not exist."
        exit 1
    fi
    
    # helper needs to be in loop too to always get relauched with correct options
    source "${HELPER}"

    extract "${MY_DIR}/${TOOLS_DIR}/${PROP_FILE}" "${SOURCE_DIR}" "${KANG}" --section "${SECTION}"

    # Handle the extra folder case
    if [[ "$PROP_FILE" == "proprietary-files_m10lte.txt" ]]; then
        process_extra_folders "$PROP_FILE"
    fi
    
    if [[ "$PROP_FILE" == "proprietary-files_a6lte.txt" ]]; then
        process_extra_folders "$PROP_FILE"
    fi

    if [[ "$PROP_FILE" == "proprietary-files_a7y17lte.txt" ]]; then
        process_extra_folders "$PROP_FILE"
    fi
    
done

# audio.primary.exynos7870.so

# breaks lib
# "${PATCHELF}" --add-needed "libaudioproxy_shim.so" "${BLOB_ROOT}/M10LTE_AUDIO/vendor/lib/hw/audio.primary.exynos7870.so"
# "${PATCHELF}" --add-needed "libaudioproxy_shim.so" "${BLOB_ROOT}/A6LTE_AUDIO/vendor/lib/hw/audio.primary.exynos7870.so"

# replace libtinyalsa with renamed one
sed -i 's|libtinyalsa.so|libalsa7870.so|g' "${BLOB_ROOT}/M10LTE_AUDIO/vendor/lib/hw/audio.primary.exynos7870.so"
sed -i 's|libtinyalsa.so|libalsa7870.so|g' "${BLOB_ROOT}/A6LTE_AUDIO/vendor/lib/hw/audio.primary.exynos7870.so"

# replace libaudioroute with renamed one
sed -i 's|libaudioroute.so|libaudior7870.so|g' "${BLOB_ROOT}/M10LTE_AUDIO/vendor/lib/hw/audio.primary.exynos7870.so"
sed -i 's|libaudioroute.so|libaudior7870.so|g' "${BLOB_ROOT}/A6LTE_AUDIO/vendor/lib/hw/audio.primary.exynos7870.so"

# libpreprocessing_nxp.so
sed -i 's|libtinyalsa.so|libalsa7870.so|g' "${BLOB_ROOT}/M10LTE_AUDIO/vendor/lib/libpreprocessing_nxp.so"
sed -i 's|libtinyalsa.so|libalsa7870.so|g' "${BLOB_ROOT}/A6LTE_AUDIO/vendor/lib/libpreprocessing_nxp.so"

# libaudior7870.so
# replace libtinyalsa with renamed one
sed -i 's|libtinyalsa.so|libalsa7870.so|g' "${BLOB_ROOT}/M10LTE_AUDIO/vendor/lib/libaudior7870.so"
sed -i 's|libtinyalsa.so|libalsa7870.so|g' "${BLOB_ROOT}/A6LTE_AUDIO/vendor/lib/libaudior7870.so"

# replace so name
sed -i 's|libaudioroute.so|libaudior7870.so|g' "${BLOB_ROOT}/M10LTE_AUDIO/vendor/lib/libaudior7870.so"
sed -i 's|libaudioroute.so|libaudior7870.so|g' "${BLOB_ROOT}/A6LTE_AUDIO/vendor/lib/libaudior7870.so"

# libalsa7870.so
# replace so name
sed -i 's|libtinyalsa.so|libalsa7870.so|g' "${BLOB_ROOT}/M10LTE_AUDIO/vendor/lib/libalsa7870.so"
sed -i 's|libtinyalsa.so|libalsa7870.so|g' "${BLOB_ROOT}/A6LTE_AUDIO/vendor/lib/libalsa7870.so"

# libril-samsung.so | setting so name with patchelf breaks the lib
#"${PATCHELF}" --set-soname "libril-samsung.so" "${BLOB_ROOT}/vendor/lib/libril-samsung.so"
#"${PATCHELF}" --set-soname "libril-samsung.so" "${BLOB_ROOT}/vendor/lib64/libril-samsung.so"

#sed -i 's|memtrack.universal7880.so|memtrack.universal7870.so|g' "${BLOB_ROOT}/ARM64_PROPRIETARY_BSP/vendor/lib64/hw/memtrack.universal7870.so"
#sed -i 's|memtrack.universal7880.so|memtrack.universal7870.so|g' "${BLOB_ROOT}/ARM64_PROPRIETARY_BSP/vendor/lib/hw/memtrack.universal7870.so"

# prebuilt bsp

# (lib64/omx/)
#sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT}/ARM64_PROPRIETARY_BSP/vendor/lib64/libExynosOMX_Core.so"
#sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT}/ARM64_PROPRIETARY_BSP/vendor/lib64/omx/libOMX.Exynos.AVC.Decoder.so"
#sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT}/ARM64_PROPRIETARY_BSP/vendor/lib64/omx/libOMX.Exynos.VP9.Decoder.so"
#sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT}/ARM64_PROPRIETARY_BSP/vendor/lib64/omx/libOMX.Exynos.HEVC.Decoder.so"
#sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT}/ARM64_PROPRIETARY_BSP/vendor/lib64/omx/libOMX.Exynos.WMV.Decoder.so"
#sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT}/ARM64_PROPRIETARY_BSP/vendor/lib64/omx/libOMX.Exynos.VP8.Decoder.so"
#sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT}/ARM64_PROPRIETARY_BSP/vendor/lib64/omx/libOMX.Exynos.MPEG4.Decoder.so"

# (lib/omx/)
#sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT}/ARM64_PROPRIETARY_BSP/vendor/lib/libExynosOMX_Core.so"
#sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT}/ARM64_PROPRIETARY_BSP/vendor/lib/omx/libOMX.Exynos.AVC.Decoder.so"
#sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT}/ARM64_PROPRIETARY_BSP/vendor/lib/omx/libOMX.Exynos.VP9.Decoder.so"
#sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT}/ARM64_PROPRIETARY_BSP/vendor/lib/omx/libOMX.Exynos.HEVC.Decoder.so"
#sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT}/ARM64_PROPRIETARY_BSP/vendor/lib/omx/libOMX.Exynos.WMV.Decoder.so"
#sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT}/ARM64_PROPRIETARY_BSP/vendor/lib/omx/libOMX.Exynos.VP8.Decoder.so"
#sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT}/ARM64_PROPRIETARY_BSP/vendor/lib/omx/libOMX.Exynos.MPEG4.Decoder.so"


#Original
#0000:60A0 |                 73 79 73  2F 64 65 76  69 63 65 73 |      sys/devices
#0000:60B0 | 2F 00 31 34  38 33 30 30  30 30 2E 64  65 63 6F 6E | /.14830000.decon
#0000:60C0 | 5F 66 2F 76  73 79 6E 63  00 31 34 38  36 30 30 30 | _f/vsync.1486000
#0000:60D0 | 30 2E 73 79  73 6D 6D 75  2F 31 34 38  36 30 30 30 | 0.sysmmu/1486000
#0000:60E0 | 30 2E 73 79  73 6D 6D 75  2F 00 65 78  79 6E 6F 73 | 0.sysmmu/.exynos
#0000:60F0 | 35 2D 66 62  2E 31 2F 76  73 79 6E 63  00 70 6C 61 | 5-fb.1/vsync.pla
#0000:6100 | 74 66 6F 72  6D 2F 65 78  79 6E 6F 73  2D 73 79 73 | tform/exynos-sys
#0000:6110 | 6D 6D 75 2E  33 30 2F 65  78 79 6E 6F  73 2D 73 79 | mmu.30/exynos-sy
#0000:6120 | 73 6D 6D 75  2E 31 31 2F  00 66 61 69  6C 65 64 20 | smmu.11/.failed 


#Changed:
#0000:60A0 |                 73 79 73  2F 64 65 76  69 63 65 73 |      sys/devices
#0000:60B0 | 2F 00 31 34  38 33 30 30  30 30 2E 64  65 63 6F 6E | /.14830000.decon
#0000:60C0 | 5F 66 62 2F  76 73 79 6E  63 00 31 34  38 35 30 30 | _fb/vsync.148500
#0000:60D0 | 30 30 2E 73  79 73 6D 6D  75 2F 31 34  38 35 30 30 | 00.sysmmu/148500
#0000:60E0 | 30 30 2E 73  79 73 6D 6D  75 2F 00 65  78 79 6E 6F | 00.sysmmu/.exyno
#0000:60F0 | 73 35 2D 66  62 2E 31 2F  76 73 79 6E  63 00 70 6C | s5-fb.1/vsync.pl
#0000:6100 | 61 74 66 6F  72 6D 2F 65  78 79 6E 6F  73 2D 73 79 | atform/exynos-sy
#0000:6110 | 73 6D 6D 75  2E 33 30 2F  65 78 79 6E  6F 73 2D 73 | smmu.30/exynos-s
#0000:6120 | 79 73 6D 6D  75 2E 31 31  2F 00 66 61  69 6C 64 20 | ysmmu.11/.faild 

# sed -i 's|\x73\x79\x73\x2F\x64\x65\x76\x69\x63\x65\x73\x2F\x00\x31\x34\x38\x33\x30\x30\x30\x30\x2E\x64\x65\x63\x6F\x6E\x5F\x66\x2F\x76\x73\x79\x6E\x63\x00\x31\x34\x38\x36\x30\x30\x30\x30\x2E\x73\x79\x73\x6D\x6D\x75\x2F\x31\x34\x38\x36\x30\x30\x30\x30\x2E\x73\x79\x73\x6D\x6D\x75\x2F\x00\x65\x78\x79\x6E\x6F\x73\x35\x2D\x66\x62\x2E\x31\x2F\x76\x73\x79\x6E\x63\x00\x70\x6C\x61\x74\x66\x6F\x72\x6D\x2F\x65\x78\x79\x6E\x6F\x73\x2D\x73\x79\x73\x6D\x6D\x75\x2E\x33\x30\x2F\x65\x78\x79\x6E\x6F\x73\x2D\x73\x79\x73\x6D\x6D\x75\x2E\x31\x31\x2F\x00\x66\x61\x69\x6C\x65\x64\x20|\x73\x79\x73\x2F\x64\x65\x76\x69\x63\x65\x73\x2F\x00\x31\x34\x38\x33\x30\x30\x30\x30\x2E\x64\x65\x63\x6F\x6E\x5F\x66\x62\x2F\x76\x73\x79\x6E\x63\x00\x31\x34\x38\x35\x30\x30\x30\x30\x2E\x73\x79\x73\x6D\x6D\x75\x2F\x31\x34\x38\x35\x30\x30\x30\x30\x2E\x73\x79\x73\x6D\x6D\x75\x2F\x00\x65\x78\x79\x6E\x6F\x73\x35\x2D\x66\x62\x2E\x31\x2F\x76\x73\x79\x6E\x63\x00\x70\x6C\x61\x74\x66\x6F\x72\x6D\x2F\x65\x78\x79\x6E\x6F\x73\x2D\x73\x79\x73\x6D\x6D\x75\x2E\x33\x30\x2F\x65\x78\x79\x6E\x6F\x73\x2D\x73\x79\x73\x6D\x6D\x75\x2E\x31\x31\x2F\x00\x66\x61\x69\x6C\x64\x20|g' "${BLOB_ROOT}/ARM64_PROPRIETARY_BSP/vendor/lib/hw/hwcomposer.exynos7870.so"

# sed -i 's|\x73\x79\x73\x2F\x64\x65\x76\x69\x63\x65\x73\x2F\x00\x31\x34\x38\x33\x30\x30\x30\x30\x2E\x64\x65\x63\x6F\x6E\x5F\x66\x2F\x76\x73\x79\x6E\x63\x00\x31\x34\x38\x36\x30\x30\x30\x30\x2E\x73\x79\x73\x6D\x6D\x75\x2F\x31\x34\x38\x36\x30\x30\x30\x30\x2E\x73\x79\x73\x6D\x6D\x75\x2F\x00\x65\x78\x79\x6E\x6F\x73\x35\x2D\x66\x62\x2E\x31\x2F\x76\x73\x79\x6E\x63\x00\x70\x6C\x61\x74\x66\x6F\x72\x6D\x2F\x65\x78\x79\x6E\x6F\x73\x2D\x73\x79\x73\x6D\x6D\x75\x2E\x33\x30\x2F\x65\x78\x79\x6E\x6F\x73\x2D\x73\x79\x73\x6D\x6D\x75\x2E\x31\x31\x2F\x00\x66\x61\x69\x6C\x65\x64\x20|\x73\x79\x73\x2F\x64\x65\x76\x69\x63\x65\x73\x2F\x00\x31\x34\x38\x33\x30\x30\x30\x30\x2E\x64\x65\x63\x6F\x6E\x5F\x66\x62\x2F\x76\x73\x79\x6E\x63\x00\x31\x34\x38\x35\x30\x30\x30\x30\x2E\x73\x79\x73\x6D\x6D\x75\x2F\x31\x34\x38\x35\x30\x30\x30\x30\x2E\x73\x79\x73\x6D\x6D\x75\x2F\x00\x65\x78\x79\x6E\x6F\x73\x35\x2D\x66\x62\x2E\x31\x2F\x76\x73\x79\x6E\x63\x00\x70\x6C\x61\x74\x66\x6F\x72\x6D\x2F\x65\x78\x79\x6E\x6F\x73\x2D\x73\x79\x73\x6D\x6D\x75\x2E\x33\x30\x2F\x65\x78\x79\x6E\x6F\x73\x2D\x73\x79\x73\x6D\x6D\x75\x2E\x31\x31\x2F\x00\x66\x61\x69\x6C\x64\x20|g' "${BLOB_ROOT}/ARM64_PROPRIETARY_BSP/vendor/lib64/hw/hwcomposer.exynos7870.so"

# rild
"${PATCHELF}" --replace-needed "libril.so" "libril-samsung.so" "${BLOB_ROOT}/vendor/bin/hw/rild"

# libsec-ril.so
"${PATCHELF}" --replace-needed "libril.so" "libril-samsung.so" "${BLOB_ROOT}/vendor/lib64/libsec-ril.so"
"${PATCHELF}" --add-needed "libcutils_shim_vendor.so" "${BLOB_ROOT}/vendor/lib64/libsec-ril.so"
"${PATCHELF}" --replace-needed "libril.so" "libril-samsung.so" "${BLOB_ROOT}/vendor/lib/libsec-ril.so" 
"${PATCHELF}" --add-needed "libcutils_shim_vendor.so" "${BLOB_ROOT}/vendor/lib/libsec-ril.so"

# libsec-ril-dsds.so
"${PATCHELF}" --replace-needed "libril.so" "libril-samsung.so" "${BLOB_ROOT}/vendor/lib64/libsec-ril-dsds.so"
"${PATCHELF}" --add-needed "libcutils_shim_vendor.so" "${BLOB_ROOT}/vendor/lib64/libsec-ril-dsds.so"
"${PATCHELF}" --replace-needed "libril.so" "libril-samsung.so" "${BLOB_ROOT}/vendor/lib/libsec-ril-dsds.so"
"${PATCHELF}" --add-needed "libcutils_shim_vendor.so" "${BLOB_ROOT}/vendor/lib/libsec-ril-dsds.so"

# camera.vendor.exynos7870.so
"${PATCHELF}" --replace-needed "libcamera_client.so" "libcamera_metadata_helper.so" "${BLOB_ROOT}/vendor/lib/hw/camera.vendor.exynos7870.so"
"${PATCHELF}" --replace-needed "libgui.so" "libgui_vendor.so" "${BLOB_ROOT}/vendor/lib/hw/camera.vendor.exynos7870.so"
"${PATCHELF}" --add-needed "libexynoscamera_shim.so" "${BLOB_ROOT}/vendor/lib/hw/camera.vendor.exynos7870.so"

# libsensorlistener.so
# shim needed by camera
"${PATCHELF}" --add-needed "libshim_sensorndkbridge.so" "${BLOB_ROOT}/vendor/lib/libsensorlistener.so"

# ffffffff00000000000000000000002f.tlbin | We are universal7870 not universal7880
#sed -i 's|universal7880|universal7870|g' "${BLOB_ROOT}/vendor/app/mcRegistry/ffffffff00000000000000000000002f.tlbin"

# ffffffff000000000000000000000041.tlbin | We are smdk7870 not smdk7880
#sed -i 's|smdk7880|smdk7870|g' "${BLOB_ROOT}/vendor/app/mcRegistry/ffffffff000000000000000000000041.tlbin"

# we are exynos7870 not exynos7880
#sed -i 's|exynos7880|exynos7870|g' "${BLOB_ROOT}/vendor/lib/libMcClient.so"
#sed -i 's|exynos7880|exynos7870|g' "${BLOB_ROOT}/vendor/lib64/libMcClient.so"

#sed -i 's|exynos7880|exynos7870|g' "${BLOB_ROOT}/vendor/lib/libMcRegistry.so"
#sed -i 's|exynos7880|exynos7870|g' "${BLOB_ROOT}/vendor/lib64/libMcRegistry.so"

"${MY_DIR}/setup-makefiles.sh"
