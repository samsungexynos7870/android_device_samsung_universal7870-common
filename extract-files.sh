#!/bin/bash
# # Copyright (C) 2017-2021 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

set -e

DEVICE_COMMON=universal7870-common
VENDOR=samsung

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

# Default to sanitizing the vendor folder before extraction
CLEAN_VENDOR=true

KANG=
SECTION=

# proprietary-files_device.txt
declare -A PROP_FILES=(
    ["proprietary-files_a7y17lte.txt"]=""
    ["proprietary-files_m10lte.txt"]=""
    ["proprietary-files_starlte.txt"]=""
    ["proprietary-files_a3y17lte-lineage-19.txt"]=""
)

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
    echo "example Usage: look at vendor-tools\universal7870-common-extract-files-example.sh"
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

for PROP_FILE in "${!PROP_FILES[@]}"; do
    SOURCE_DIR=${PROP_FILES[$PROP_FILE]}
    # Check if any source directory with proprietary-files is empty.
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

    extract "${MY_DIR}/vendor-tools/${PROP_FILE}" "${SOURCE_DIR}" "${KANG}" --section "${SECTION}"
    
done

# BLOB_ROOT
BLOB_ROOT="${ANDROID_ROOT}"/vendor/"${VENDOR}"/"${DEVICE_COMMON}"/proprietary/vendor

for lib in libsensorlistener.so; do
    if [[ -f "${BLOB_ROOT}/lib/${lib}" ]]; then
        "$PATCHELF" --add-needed "libshim_sensorndkbridge.so" "${BLOB_ROOT}/lib/${lib}"
    fi
    
    if [[ -f "${BLOB_ROOT}/lib64/${lib}" ]]; then
        "$PATCHELF" --add-needed "libshim_sensorndkbridge.so" "${BLOB_ROOT}/lib64/${lib}"
    fi
done

#for lib in rild; do
#    if [[ -f "${BLOB_ROOT}/bin/hw/${lib}" ]]; then
#        "$PATCHELF" --replace-needed "libril.so" "libril-samsung.so" "${BLOB_ROOT}/bin/hw/${lib}"
#    fi
#done

for lib in libsec-ril.so ; do
    
#    if [[ -f "${BLOB_ROOT}/lib64/${lib}" ]]; then
#        "$PATCHELF" --replace-needed "libril.so" "libril-samsung.so" "${BLOB_ROOT}/lib64/${lib}"
#    fi
    
    if [[ -f "${BLOB_ROOT}/lib64/${lib}" ]]; then
        "$PATCHELF" --add-needed "libcutils_shim_vendor.so" "${BLOB_ROOT}/lib64/${lib}"
    fi
    
    
done

for lib in libsec-ril-dsds.so ; do
    
#    if [[ -f "${BLOB_ROOT}/lib64/${lib}" ]]; then
#        "$PATCHELF" --replace-needed "libril.so" "libril-samsung.so" "${BLOB_ROOT}/lib64/${lib}"
#    fi
    
    if [[ -f "${BLOB_ROOT}/lib64/${lib}" ]]; then
        "$PATCHELF" --add-needed "libcutils_shim_vendor.so" "${BLOB_ROOT}/lib64/${lib}"
    fi
done

"${MY_DIR}/setup-makefiles.sh"
