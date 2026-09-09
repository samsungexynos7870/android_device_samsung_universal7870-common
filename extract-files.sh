#!/bin/bash
# # Copyright (C) 2017-2024 The LineageOS Project
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

# VENDOR_MK_ROOT
VENDOR_MK_ROOT_INTERNAL="${ANDROID_ROOT}"/vendor/"${VENDOR}"
VENDOR_MK_ROOT="${ANDROID_ROOT}"/vendor/"${VENDOR}"/"${DEVICE_COMMON}"
#INTERNAL_VENDOR_MK_ROOT_AUDIO_M10LTE="${ANDROID_ROOT}"/vendor/"${VENDOR}"/"${INTERNAL_DEVICE_COMMON_AUDIO_M10LTE}"
#INTERNAL_VENDOR_MK_ROOT_AUDIO_A6LTE="${ANDROID_ROOT}"/vendor/"${VENDOR}"/"${INTERNAL_DEVICE_COMMON_AUDIO_A6LTE}"
#...

# BLOB_ROOT
BLOB_ROOT="${VENDOR_MK_ROOT}"/proprietary
#BLOB_ROOT_AUDIO_M10LTE="${INTERNAL_VENDOR_MK_ROOT_AUDIO_M10LTE}"/proprietary
#BLOB_ROOT_AUDIO_A6LTE="${INTERNAL_VENDOR_MK_ROOT_AUDIO_A6LTE}"/proprietary
#BLOB_ROOT_M10LTE="${INTERNAL_VENDOR_MK_ROOT_M10LTE}"/proprietary
#...

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
    # Declare INTERNAL_DEVICE_COMMON as a global associative array
    declare -gA INTERNAL_DEVICE_COMMON

    # Declare PROP_CODENAMES as a global associative array
    declare -gA PROP_CODENAMES

    # List all 'proprietary-files_*.txt' files in the vendor-tools directory
    local files=(${vendor_tools_dir}/proprietary-files_*.txt)
    for file_path in "${files[@]}"; do
        if [[ -f "$file_path" ]]; then
            local filename=$(basename "$file_path")
            # Add to PROP_FILES associative array with empty value            
            PROP_FILES["$filename"]=""

            # Extract the part after 'proprietary-files_' and before '.txt'
            local name_part=${filename#proprietary-files_}
            name_part=${name_part%.txt}
            # Add to INTERNAL_DEVICE_COMMON associative array with filename as key
            INTERNAL_DEVICE_COMMON["$filename"]="$name_part"

            # Extract and process codename
            local codename=$(echo "$name_part" | cut -d'_' -f1)
            if [[ "$codename" != "common" ]]; then
                PROP_CODENAMES["$codename"]=""
            fi
        fi
    done

    # Generate INTERNAL_VENDOR_MK_ROOT
    for key in "${!INTERNAL_DEVICE_COMMON[@]}"; do
        name="${INTERNAL_DEVICE_COMMON[$key]}"
        root_name="INTERNAL_VENDOR_MK_ROOT_${name}"

        declare -g "${root_name}=${ANDROID_ROOT}/vendor/${VENDOR}/${name}"
    done
    
}

split_files() {
    local codename="$1"
    local base_dir="${MY_DIR}/${TOOLS_DIR}"
    local codename_dir="${base_dir}/${codename}"
    mkdir -p "$codename_dir"  # Ensure the codename directory exists

    # Variable to hold the current file name, empty initially
    current_file=""

    # Read proprietary-files.txt line by line
    while IFS= read -r line; do
        # Check if the line is a file marker
        if [[ "$line" =~ ^###FILE###:(.*) ]]; then
            # Extract the file name from the marker
            current_file="${BASH_REMATCH[1]}"
            # Skip file creation if the file name is empty (should not happen)
            if [ -z "$current_file" ]; then
                continue
            fi
            # Start writing to the new file, creating it or overwriting if it already exists
            # Ensure files are created in the codename directory
            echo -n "" > "${codename_dir}/${current_file}"
        else
            # Append the line to the current file, if there is one
            if [ -n "$current_file" ]; then
                echo "$line" >> "${codename_dir}/${current_file}"
            fi
        fi
    done < "${base_dir}/proprietary-files_${codename}.txt"
}


generate_prop_files_array "${MY_DIR}/${TOOLS_DIR}"

echo "Printing PROP_FILES array:"
for filename in "${!PROP_FILES[@]}"; do
    echo "Filename: $filename"
done

echo "Printing INTERNAL_DEVICE_COMMON array:"
for filename in "${!INTERNAL_DEVICE_COMMON[@]}"; do
    echo "Filename: $filename, Extracted Name: ${INTERNAL_DEVICE_COMMON[$filename]}"
done

echo "Printing PROP_CODENAMES array:"

for key in "${!INTERNAL_DEVICE_COMMON[@]}"; do
    common_name="${INTERNAL_DEVICE_COMMON[$key]}"
    
    mk_root_varname="INTERNAL_VENDOR_MK_ROOT_${common_name}"
    blob_root_varname="BLOB_ROOT_${common_name}"
    
    # Using indirect variable reference to get the values
    echo "$mk_root_varname:"
    echo "${!mk_root_varname}"
    echo "$blob_root_varname:"
    echo "${!mk_root_varname}/proprietary"
done

for codename in "${!PROP_CODENAMES[@]}"; do
    echo "Codename: $codename"
done

echo ""

for codename in "${!PROP_CODENAMES[@]}"; do
    base_dir="${MY_DIR}/${TOOLS_DIR}/"
    codename_dir="${base_dir}/${codename}"
    mkdir -p "$codename_dir"
        echo "Processing codename: $codename"
    split_files "$codename"
    generate_prop_files_array "$codename_dir"
done


for key in "${!INTERNAL_DEVICE_COMMON[@]}"; do
    common_name="${INTERNAL_DEVICE_COMMON[$key]}"
    
    mk_root_varname="INTERNAL_VENDOR_MK_ROOT_${common_name}"
    blob_root_varname="BLOB_ROOT_${common_name}"
    
    # Using indirect variable reference to get the values
    echo "$mk_root_varname:"
    echo "${!mk_root_varname}"
    echo "$blob_root_varname:"
    echo "${!mk_root_varname}/proprietary"
done

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

for PROP_FILE in "${!PROP_FILES[@]}"; do
    if [[ "$PROP_FILE" == "proprietary-files_m10lte.txt" ]]; then
    SOURCE_DIR_M10LTE=${PROP_FILES[$PROP_FILE]}
    fi
    if [[ "$PROP_FILE" == "proprietary-files_a6lte.txt" ]]; then
    SOURCE_DIR_A6LTE=${PROP_FILES[$PROP_FILE]}
    fi
    if [[ "$PROP_FILE" == "proprietary-files_a6ltep.txt" ]]; then
    SOURCE_DIR_A6LTE_P=${PROP_FILES[$PROP_FILE]}
    fi
    if [[ "$PROP_FILE" == "proprietary-files_j7duolte.txt" ]]; then
    SOURCE_DIR_J7DUOLTE=${PROP_FILES[$PROP_FILE]}
    fi
    if [[ "$PROP_FILE" == "proprietary-files_a7y17lte.txt" ]]; then
    SOURCE_DIR_A7Y17LTE=${PROP_FILES[$PROP_FILE]}
    fi
    if [[ "$PROP_FILE" == "proprietary-files_gracerlte.txt" ]]; then
    SOURCE_DIR_GRACERLTE=${PROP_FILES[$PROP_FILE]}
    fi
    if [[ "$PROP_FILE" == "proprietary-files_oss.txt" ]]; then
    SOURCE_DIR_OSS_PREBUILT=${PROP_FILES[$PROP_FILE]}
    fi
done

for PROP_FILE in "${!PROP_FILES[@]}"; do

    COMMON_NAME="${INTERNAL_DEVICE_COMMON[$PROP_FILE]}"
    if [ -z "$COMMON_NAME" ]; then
    COMMON_NAME="dummy"
    fi
    echo "Internal Codename: $COMMON_NAME"
    setup_vendor "${COMMON_NAME}" "${VENDOR}" "${ANDROID_ROOT}" true "${CLEAN_VENDOR}"
     
    if [[ "$PROP_FILE" == proprietary-files_m10lte*.txt ]]; then
    extract "${MY_DIR}/${TOOLS_DIR}/m10lte/${PROP_FILE}" "${SOURCE_DIR_M10LTE}" "${KANG}" --section "${SECTION}"
    fi
    if [[ "$PROP_FILE" == proprietary-files_j7duolte*.txt ]]; then
    extract "${MY_DIR}/${TOOLS_DIR}/j7duolte/${PROP_FILE}" "${SOURCE_DIR_J7DUOLTE}" "${KANG}" --section "${SECTION}"
    fi
    if [[ "$PROP_FILE" == proprietary-files_a6ltep*.txt ]]; then
    if [[ "$PROP_FILE" != proprietary-files_a6ltep.txt ]]; then
    extract "${MY_DIR}/${TOOLS_DIR}/a6ltep/${PROP_FILE}" "${SOURCE_DIR_A6LTE}" "${KANG}" --section "${SECTION}"
    fi
    fi
    if [[ "$PROP_FILE" == proprietary-files_a6lte*.txt ]]; then
    if [[ "$PROP_FILE" != proprietary-files_a6lte.txt ]]; then
    if [[ "$PROP_FILE" != proprietary-files_a6ltep_bsp_p.txt ]]; then
    if [[ "$PROP_FILE" != proprietary-files_a6ltep.txt ]]; then
    if [[ "$PROP_FILE" != proprietary-files_a6lte_p.txt ]]; then
    extract "${MY_DIR}/${TOOLS_DIR}/a6lte/${PROP_FILE}" "${SOURCE_DIR_A6LTE}" "${KANG}" --section "${SECTION}"
    fi
    fi
    fi
    fi
    fi
    if [[ "$PROP_FILE" == proprietary-files_a7y17lte*.txt ]]; then
    extract "${MY_DIR}/${TOOLS_DIR}/a7y17lte/${PROP_FILE}" "${SOURCE_DIR_A7Y17LTE}" "${KANG}" --section "${SECTION}"
    fi
    if [[ "$PROP_FILE" == proprietary-files_gracerlte_bsp_p.txt ]]; then
    extract "${MY_DIR}/${TOOLS_DIR}/gracerlte/${PROP_FILE}" "${SOURCE_DIR_GRACERLTE}" "${KANG}" --section "${SECTION}"
    fi
    if [[ "$PROP_FILE" == proprietary-files_oss_hwc.txt ]]; then
    extract "${MY_DIR}/${TOOLS_DIR}/oss/${PROP_FILE}" "${SOURCE_DIR_OSS_PREBUILT}" "${KANG}" --section "${SECTION}"
    fi
    
done


for key in "${!INTERNAL_DEVICE_COMMON[@]}"; do
    COMMON_NAME="${INTERNAL_DEVICE_COMMON[$key]}"
    
    mk_root_varname="INTERNAL_VENDOR_MK_ROOT_${COMMON_NAME}"
    blob_root_varname="BLOB_ROOT_${COMMON_NAME}"
    
    # Using indirect variable reference to get the values
    #echo "$mk_root_varname:"
    #echo "${!mk_root_varname}"
    #echo "$blob_root_varname:"
    #echo "${!mk_root_varname}/proprietary"
    
    if [[ "$COMMON_NAME" == m10lte_audio ]]; then
    BLOB_ROOT_AUDIO_M10LTE="${!mk_root_varname}/proprietary"
    echo "Patching files in: ${BLOB_ROOT_AUDIO_M10LTE}"
    
    # replace libtinyalsa with renamed one
    sed -i 's|libtinyalsa.so|libalsa7870.so|g' "${BLOB_ROOT_AUDIO_M10LTE}/vendor/lib/hw/audio.primary.exynos7870.so"

    # replace libaudioroute with renamed one
    sed -i 's|libaudioroute.so|libaudior7870.so|g' "${BLOB_ROOT_AUDIO_M10LTE}/vendor/lib/hw/audio.primary.exynos7870.so"

    # libpreprocessing_nxp.so
    sed -i 's|libtinyalsa.so|libalsa7870.so|g' "${BLOB_ROOT_AUDIO_M10LTE}/vendor/lib/libpreprocessing_nxp.so"

    fi
    
    if [[ "$COMMON_NAME" == a6lte_audio ]]; then
    BLOB_ROOT_AUDIO_A6LTE="${!mk_root_varname}/proprietary"
    echo "Patching files in: ${BLOB_ROOT_AUDIO_A6LTE}"

    # replace libtinyalsa with renamed one
    sed -i 's|libtinyalsa.so|libalsa7870.so|g' "${BLOB_ROOT_AUDIO_A6LTE}/vendor/lib/hw/audio.primary.exynos7870.so"

    # replace libaudioroute with renamed one
    sed -i 's|libaudioroute.so|libaudior7870.so|g' "${BLOB_ROOT_AUDIO_A6LTE}/vendor/lib/hw/audio.primary.exynos7870.so"

    # libpreprocessing_nxp.so
    sed -i 's|libtinyalsa.so|libalsa7870.so|g' "${BLOB_ROOT_AUDIO_A6LTE}/vendor/lib/libpreprocessing_nxp.so"

    fi
    


    # audio.primary.exynos7870.so

    # check path

    if [[ "$COMMON_NAME" == a7y17lte_bsp ]]; then
    BLOB_ROOT_A7Y17LTE_BSP="${!mk_root_varname}/proprietary"
    echo "Patching files in: ${BLOB_ROOT_A7Y17LTE_BSP}"

    # libril-samsung.so | setting so name with patchelf breaks the lib
    #"${PATCHELF}" --set-soname "libril-samsung.so" "${BLOB_ROOT}/vendor/lib/libril-samsung.so"
    #"${PATCHELF}" --set-soname "libril-samsung.so" "${BLOB_ROOT}/vendor/lib64/libril-samsung.so"

    sed -i 's|memtrack.universal7880.so|memtrack.universal7870.so|g' "${BLOB_ROOT_A7Y17LTE_BSP}/vendor/lib64/hw/memtrack.exynos7870.so"
    sed -i 's|memtrack.universal7880.so|memtrack.universal7870.so|g' "${BLOB_ROOT_A7Y17LTE_BSP}/vendor/lib/hw/memtrack.exynos7870.so"

    # prebuilt bsp

    # (lib64/omx/)
    #sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT_A7Y17LTE_BSP}/vendor/lib64/libExynosOMX_Core.so"
    #sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT_A7Y17LTE_BSP}/vendor/lib64/omx/libOMX.Exynos.AVC.Decoder.so"
    #sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT_A7Y17LTE_BSP}/vendor/lib64/omx/libOMX.Exynos.VP9.Decoder.so"
    #sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT_A7Y17LTE_BSP}/vendor/lib64/omx/libOMX.Exynos.HEVC.Decoder.so"
    #sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT_A7Y17LTE_BSP}/vendor/lib64/omx/libOMX.Exynos.WMV.Decoder.so"
    #sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT_A7Y17LTE_BSP}/vendor/lib64/omx/libOMX.Exynos.VP8.Decoder.so"
    #sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT_A7Y17LTE_BSP}/vendor/lib64/omx/libOMX.Exynos.MPEG4.Decoder.so"

    # (lib/omx/)
    #sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT_A7Y17LTE_BSP}/vendor/lib/libExynosOMX_Core.so"
    #sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT_A7Y17LTE_BSP}/vendor/lib/omx/libOMX.Exynos.AVC.Decoder.so"
    #sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT_A7Y17LTE_BSP}/vendor/lib/omx/libOMX.Exynos.VP9.Decoder.so"
    #sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT_A7Y17LTE_BSP}/vendor/lib/omx/libOMX.Exynos.HEVC.Decoder.so"
    #sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT_A7Y17LTE_BSP}/vendor/lib/omx/libOMX.Exynos.WMV.Decoder.so"
    #sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT_A7Y17LTE_BSP}/vendor/lib/omx/libOMX.Exynos.VP8.Decoder.so"
    #sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT_A7Y17LTE_BSP}/vendor/lib/omx/libOMX.Exynos.MPEG4.Decoder.so"
#

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

    sed -i 's|\x73\x79\x73\x2F\x64\x65\x76\x69\x63\x65\x73\x2F\x00\x31\x34\x38\x33\x30\x30\x30\x30\x2E\x64\x65\x63\x6F\x6E\x5F\x66\x2F\x76\x73\x79\x6E\x63\x00\x31\x34\x38\x36\x30\x30\x30\x30\x2E\x73\x79\x73\x6D\x6D\x75\x2F\x31\x34\x38\x36\x30\x30\x30\x30\x2E\x73\x79\x73\x6D\x6D\x75\x2F\x00\x65\x78\x79\x6E\x6F\x73\x35\x2D\x66\x62\x2E\x31\x2F\x76\x73\x79\x6E\x63\x00\x70\x6C\x61\x74\x66\x6F\x72\x6D\x2F\x65\x78\x79\x6E\x6F\x73\x2D\x73\x79\x73\x6D\x6D\x75\x2E\x33\x30\x2F\x65\x78\x79\x6E\x6F\x73\x2D\x73\x79\x73\x6D\x6D\x75\x2E\x31\x31\x2F\x00\x66\x61\x69\x6C\x65\x64\x20|\x73\x79\x73\x2F\x64\x65\x76\x69\x63\x65\x73\x2F\x00\x31\x34\x38\x33\x30\x30\x30\x30\x2E\x64\x65\x63\x6F\x6E\x5F\x66\x62\x2F\x76\x73\x79\x6E\x63\x00\x31\x34\x38\x35\x30\x30\x30\x30\x2E\x73\x79\x73\x6D\x6D\x75\x2F\x31\x34\x38\x35\x30\x30\x30\x30\x2E\x73\x79\x73\x6D\x6D\x75\x2F\x00\x65\x78\x79\x6E\x6F\x73\x35\x2D\x66\x62\x2E\x31\x2F\x76\x73\x79\x6E\x63\x00\x70\x6C\x61\x74\x66\x6F\x72\x6D\x2F\x65\x78\x79\x6E\x6F\x73\x2D\x73\x79\x73\x6D\x6D\x75\x2E\x33\x30\x2F\x65\x78\x79\x6E\x6F\x73\x2D\x73\x79\x73\x6D\x6D\x75\x2E\x31\x31\x2F\x00\x66\x61\x69\x6C\x64\x20|g' "${BLOB_ROOT_A7Y17LTE_BSP}/vendor/lib/hw/hwcomposer.exynos7870.so"

    # sed -i 's|\x73\x79\x73\x2F\x64\x65\x76\x69\x63\x65\x73\x2F\x00\x31\x34\x38\x33\x30\x30\x30\x30\x2E\x64\x65\x63\x6F\x6E\x5F\x66\x2F\x76\x73\x79\x6E\x63\x00\x31\x34\x38\x36\x30\x30\x30\x30\x2E\x73\x79\x73\x6D\x6D\x75\x2F\x31\x34\x38\x36\x30\x30\x30\x30\x2E\x73\x79\x73\x6D\x6D\x75\x2F\x00\x65\x78\x79\x6E\x6F\x73\x35\x2D\x66\x62\x2E\x31\x2F\x76\x73\x79\x6E\x63\x00\x70\x6C\x61\x74\x66\x6F\x72\x6D\x2F\x65\x78\x79\x6E\x6F\x73\x2D\x73\x79\x73\x6D\x6D\x75\x2E\x33\x30\x2F\x65\x78\x79\x6E\x6F\x73\x2D\x73\x79\x73\x6D\x6D\x75\x2E\x31\x31\x2F\x00\x66\x61\x69\x6C\x65\x64\x20|\x73\x79\x73\x2F\x64\x65\x76\x69\x63\x65\x73\x2F\x00\x31\x34\x38\x33\x30\x30\x30\x30\x2E\x64\x65\x63\x6F\x6E\x5F\x66\x62\x2F\x76\x73\x79\x6E\x63\x00\x31\x34\x38\x35\x30\x30\x30\x30\x2E\x73\x79\x73\x6D\x6D\x75\x2F\x31\x34\x38\x35\x30\x30\x30\x30\x2E\x73\x79\x73\x6D\x6D\x75\x2F\x00\x65\x78\x79\x6E\x6F\x73\x35\x2D\x66\x62\x2E\x31\x2F\x76\x73\x79\x6E\x63\x00\x70\x6C\x61\x74\x66\x6F\x72\x6D\x2F\x65\x78\x79\x6E\x6F\x73\x2D\x73\x79\x73\x6D\x6D\x75\x2E\x33\x30\x2F\x65\x78\x79\x6E\x6F\x73\x2D\x73\x79\x73\x6D\x6D\x75\x2E\x31\x31\x2F\x00\x66\x61\x69\x6C\x64\x20|g' "${BLOB_ROOT_A7Y17LTE_BSP}/vendor/lib64/hw/hwcomposer.exynos7870.so"

    fi

    if [[ "$COMMON_NAME" == gracerlte ]]; then
    BLOB_ROOT_GRACERLTE_BSP="${!mk_root_varname}/proprietary"
    echo "Patching files in: ${BLOB_ROOT_GRACERLTE_BSP}"
    
    # (lib64/omx/) no patches needed here
    #sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT_GRACERLTE_BSP}/vendor/lib64/libExynosOMX_Core.so"
    #sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT_GRACERLTE_BSP}/vendor/lib64/omx/libOMX.Exynos.AVC.Decoder.so"
    #sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT_GRACERLTE_BSP}/vendor/lib64/omx/libOMX.Exynos.VP9.Decoder.so"
    #sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT_GRACERLTE_BSP}/vendor/lib64/omx/libOMX.Exynos.HEVC.Decoder.so"
    #sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT_GRACERLTE_BSP}/vendor/lib64/omx/libOMX.Exynos.WMV.Decoder.so"
    #sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT_GRACERLTE_BSP}/vendor/lib64/omx/libOMX.Exynos.VP8.Decoder.so"
    #sed -i 's|system/lib64|vendor/lib64|g' "${BLOB_ROOT_GRACERLTE_BSP}/vendor/lib64/omx/libOMX.Exynos.MPEG4.Decoder.so"

    # (lib/omx/)
    #sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT_GRACERLTE_BSP}/vendor/lib/libExynosOMX_Core.so"
    #sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT_GRACERLTE_BSP}/vendor/lib/omx/libOMX.Exynos.AVC.Decoder.so"
    #sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT_GRACERLTE_BSP}/vendor/lib/omx/libOMX.Exynos.VP9.Decoder.so"
    #sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT_GRACERLTE_BSP}/vendor/lib/omx/libOMX.Exynos.HEVC.Decoder.so"
    #sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT_GRACERLTE_BSP}/vendor/lib/omx/libOMX.Exynos.WMV.Decoder.so"
    #sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT_GRACERLTE_BSP}/vendor/lib/omx/libOMX.Exynos.VP8.Decoder.so"
    #sed -i 's|system/lib|vendor/lib|g' "${BLOB_ROOT_GRACERLTE_BSP}/vendor/lib/omx/libOMX.Exynos.MPEG4.Decoder.so"

    # Add other gracerlte specific patches as needed
    fi

    if [[ "$COMMON_NAME" == j7duolte_radio ]]; then
    BLOB_ROOT_J7DUOLTE_SEC_RADIO="${!mk_root_varname}/proprietary"
    echo "Patching files in: ${BLOB_ROOT_J7DUOLTE_SEC_RADIO}"

    # rild
    "${PATCHELF}" --replace-needed "libril.so" "libril-samsung.so" "${BLOB_ROOT_J7DUOLTE_SEC_RADIO}/vendor/bin/hw/rild"

    # libsec-ril.so
    "${PATCHELF}" --replace-needed "libril.so" "libril-samsung.so" "${BLOB_ROOT_J7DUOLTE_SEC_RADIO}/vendor/lib64/libsec-ril.so"
    "${PATCHELF}" --add-needed "libcutils_shim_vendor.so" "${BLOB_ROOT_J7DUOLTE_SEC_RADIO}/vendor/lib64/libsec-ril.so"
    "${PATCHELF}" --replace-needed "libril.so" "libril-samsung.so" "${BLOB_ROOT_J7DUOLTE_SEC_RADIO}/vendor/lib/libsec-ril.so" 
    "${PATCHELF}" --add-needed "libcutils_shim_vendor.so" "${BLOB_ROOT_J7DUOLTE_SEC_RADIO}/vendor/lib/libsec-ril.so"

    # libsec-ril-dsds.so
    "${PATCHELF}" --replace-needed "libril.so" "libril-samsung.so" "${BLOB_ROOT_J7DUOLTE_SEC_RADIO}/vendor/lib64/libsec-ril-dsds.so"
    "${PATCHELF}" --add-needed "libcutils_shim_vendor.so" "${BLOB_ROOT_J7DUOLTE_SEC_RADIO}/vendor/lib64/libsec-ril-dsds.so"
    "${PATCHELF}" --replace-needed "libril.so" "libril-samsung.so" "${BLOB_ROOT_J7DUOLTE_SEC_RADIO}/vendor/lib/libsec-ril-dsds.so"
    "${PATCHELF}" --add-needed "libcutils_shim_vendor.so" "${BLOB_ROOT_J7DUOLTE_SEC_RADIO}/vendor/lib/libsec-ril-dsds.so"
    fi

    if [[ "$COMMON_NAME" == m10lte ]]; then
    BLOB_ROOT_M10LTE="${!mk_root_varname}/proprietary"
    echo "Patching files in: ${BLOB_ROOT_M10LTE}"

    # camera.vendor.exynos7870.so
    "${PATCHELF}" --replace-needed "libcamera_client.so" "libcamera_metadata_helper.so" "${BLOB_ROOT_M10LTE}/vendor/lib/hw/camera.vendor.exynos7870.so"
    "${PATCHELF}" --replace-needed "libgui.so" "libgui_vendor.so" "${BLOB_ROOT_M10LTE}/vendor/lib/hw/camera.vendor.exynos7870.so"
    "${PATCHELF}" --add-needed "libexynoscamera_shim.so" "${BLOB_ROOT_M10LTE}/vendor/lib/hw/camera.vendor.exynos7870.so"

    # libsensorlistener.so
    # shim needed by camera
    "${PATCHELF}" --add-needed "libshim_sensorndkbridge.so" "${BLOB_ROOT_M10LTE}/vendor/lib/libsensorlistener.so"
    fi
    
    if [[ "$COMMON_NAME" == a7y17lte ]]; then
    BLOB_ROOT_A7Y17LTE="${!mk_root_varname}/proprietary"
    echo "Patching files in: ${BLOB_ROOT_A7Y17LTE}"

    # Replace protobuf with vndk29 compat libs for specified libs
    "${PATCHELF}" --replace-needed libprotobuf-cpp-lite.so libprotobuf-cpp-lite-v29.so "${BLOB_ROOT_A7Y17LTE}/vendor/lib/libwvhidl.so"
    "${PATCHELF}" --replace-needed libprotobuf-cpp-lite.so libprotobuf-cpp-lite-v29.so "${BLOB_ROOT_A7Y17LTE}/vendor/lib/mediadrm/libwvdrmengine.so"
    fi
done

"${MY_DIR}/setup-makefiles.sh"
