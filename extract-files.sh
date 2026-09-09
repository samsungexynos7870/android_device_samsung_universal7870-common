#!/bin/bash
# # Copyright (C) 2017-2026 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

# Default values (can be overridden by arguments)
DEVICE_COMMON=${1:-???}
PROPRIETARY_FILES=${2:-???}
SRC=${3:-???}
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

function blob_fixup() {
    case "${1}" in
        vendor/lib/hw/audio.primary.exynos7870.so)

        case "${2}" in
            */sec/*)
            # Fix audio.primary.exynos7870.so sec
            sed -i 's|libtinyalsa.so|libalsa7870.so|g' "${2}"
            sed -i 's|libaudioroute.so|libaudior7870.so|g' "${2}"
            ;;
            */sec_tfa/*)
            # Fix audio.primary.exynos7870.so sec tfa
            sed -i 's|libtinyalsa.so|libalsa7870.so|g' "${2}"
            sed -i 's|libaudioroute.so|libaudior7870.so|g' "${2}"
            ;;
        esac

        ;;
        vendor/lib/libpreprocessing_nxp.so)
            case "${2}" in
                */sec/*)
                # Fix libpreprocessing_nxp.so
                sed -i 's|libtinyalsa.so|libalsa7870.so|g' "${2}"
                ;;
                */sec_tfa/*)
                # Fix libpreprocessing_nxp.so
                sed -i 's|libtinyalsa.so|libalsa7870.so|g' "${2}"
                ;;
            esac
        
        ;;
        
        vendor/lib/hw/camera.vendor.exynos7870.so)
            # Fix camera.vendor.exynos7870.so
            "${PATCHELF}" --replace-needed "libcamera_client.so" "libcamera_metadata_helper.so" "${2}"
            "${PATCHELF}" --replace-needed "libgui.so" "libgui_vendor.so" "${2}"
            "${PATCHELF}" --add-needed "libexynoscamera_shim.so" "${2}"
        ;;

        vendor/lib*/hw/memtrack.exynos7870.so)
            # Fix memtrack for both lib and lib64
            sed -i 's|memtrack.universal7880.so|memtrack.universal7870.so|g' "${2}"
        ;;


        vendor/lib/hw/hwcomposer.exynos7870.so)
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

            sed -i 's|\x73\x79\x73\x2F\x64\x65\x76\x69\x63\x65\x73\x2F\x00\x31\x34\x38\x33\x30\x30\x30\x30\x2E\x64\x65\x63\x6F\x6E\x5F\x66\x2F\x76\x73\x79\x6E\x63\x00\x31\x34\x38\x36\x30\x30\x30\x30\x2E\x73\x79\x73\x6D\x6D\x75\x2F\x31\x34\x38\x36\x30\x30\x30\x30\x2E\x73\x79\x73\x6D\x6D\x75\x2F\x00\x65\x78\x79\x6E\x6F\x73\x35\x2D\x66\x62\x2E\x31\x2F\x76\x73\x79\x6E\x63\x00\x70\x6C\x61\x74\x66\x6F\x72\x6D\x2F\x65\x78\x79\x6E\x6F\x73\x2D\x73\x79\x73\x6D\x6D\x75\x2E\x33\x30\x2F\x65\x78\x79\x6E\x6F\x73\x2D\x73\x79\x73\x6D\x6D\x75\x2E\x31\x31\x2F\x00\x66\x61\x69\x6C\x65\x64\x20|\x73\x79\x73\x2F\x64\x65\x76\x69\x63\x65\x73\x2F\x00\x31\x34\x38\x33\x30\x30\x30\x30\x2E\x64\x65\x63\x6F\x6E\x5F\x66\x62\x2F\x76\x73\x79\x6E\x63\x00\x31\x34\x38\x35\x30\x30\x30\x30\x2E\x73\x79\x73\x6D\x6D\x75\x2F\x31\x34\x38\x35\x30\x30\x30\x30\x2E\x73\x79\x73\x6D\x6D\x75\x2F\x00\x65\x78\x79\x6E\x6F\x73\x35\x2D\x66\x62\x2E\x31\x2F\x76\x73\x79\x6E\x63\x00\x70\x6C\x61\x74\x66\x6F\x72\x6D\x2F\x65\x78\x79\x6E\x6F\x73\x2D\x73\x79\x73\x6D\x6D\x75\x2E\x33\x30\x2F\x65\x78\x79\x6E\x6F\x73\x2D\x73\x79\x73\x6D\x6D\x75\x2E\x31\x31\x2F\x00\x66\x61\x69\x6C\x64\x20|g' "${2}"
        ;;
        vendor/bin/hw/rild)
            # Fix rild
            "${PATCHELF}" --replace-needed "libril.so" "libril-samsung.so" "${2}"
        ;;
        
        vendor/lib*/libsec-ril.so)
            # Fix libsec-ril.so for both lib and lib64
            "${PATCHELF}" --replace-needed "libril.so" "libril-samsung.so" "${2}"
            "${PATCHELF}" --add-needed "libcutils_shim_vendor.so" "${2}"
        ;;
        
        vendor/lib*/libsec-ril-dsds.so)
            # Fix libsec-ril-dsds.so for both lib and lib64
            "${PATCHELF}" --replace-needed "libril.so" "libril-samsung.so" "${2}"
            "${PATCHELF}" --add-needed "libcutils_shim_vendor.so" "${2}"
        ;;


        vendor/lib/libsensorlistener.so)
            # shim needed by camera lib
            "${PATCHELF}" --add-needed "libshim_sensorndkbridge.so" "${2}"
        ;;
        
        vendor/lib*/libwvhidl.so)
            # Replace protobuf with vndk29 compat lib both lib and lib64
            "${PATCHELF}" --replace-needed libprotobuf-cpp-lite.so libprotobuf-cpp-lite-v29.so "${2}"
        ;;
        
        vendor/lib*/mediadrm/libwvdrmengine.so)
            # Replace protobuf with vndk29 compat lib both lib and lib64
            "${PATCHELF}" --replace-needed libprotobuf-cpp-lite.so libprotobuf-cpp-lite-v29.so "${2}"
        ;;
    esac
}

# Default to sanitizing the vendor folder before extraction
CLEAN_VENDOR=true

KANG=
SECTION=
SETUP_MAKEFILES_ARGS=()

# Parse command line arguments starting from the third one
shift 2
while [ "${#}" -gt 0 ]; do
    case "${1}" in
        -n | --no-cleanup )
                CLEAN_VENDOR=false
                SETUP_MAKEFILES_ARGS+=(--no-cleanup)
                ;;
        -k | --kang )
                KANG="--kang"
                ;;
        -s | --section )
                SECTION="${2}"; shift
                CLEAN_VENDOR=false
                SETUP_MAKEFILES_ARGS+=(--section "${SECTION}")
                ;;
        * )
                SRC="${1}"
                ;;
    esac
    shift
done

if [ -z "${SRC}" ]; then
    SRC="adb"
fi

# Initialize the helper
setup_vendor "${DEVICE_COMMON}" "${VENDOR}" "${ANDROID_ROOT}" true "${CLEAN_VENDOR}"

extract "${MY_DIR}/${PROPRIETARY_FILES}" "${SRC}" "${KANG}" --section "${SECTION}"


"${MY_DIR}/setup-makefiles.sh" "${DEVICE_COMMON}" "${PROPRIETARY_FILES}" "${SETUP_MAKEFILES_ARGS[@]}"
