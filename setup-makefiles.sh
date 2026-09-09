#!/bin/bash
#
# Copyright (C) 2017-2024 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

set -e

# new codename
DEVICE_COMMON=universal7870-common
VENDOR=samsung
VENDOR_UNIVERSAL7870_COMMON="${VENDOR}/universal7870-common"
TOOLS_DIR="vendor-tools"

OUTDIR=vendor/$VENDOR/$DEVICE_COMMON

export INITIAL_COPYRIGHT_YEAR=2017

# Load extract_utils and do some sanity checks
MY_DIR="${BASH_SOURCE%/*}"
if [[ ! -d "${MY_DIR}" ]]; then MY_DIR="${PWD}"; fi

OUTDIR=vendor/$VENDOR/$DEVICE_COMMON

ANDROID_ROOT="${MY_DIR}/../../.."
HELPER="${ANDROID_ROOT}/tools/extract-utils/extract_utils.sh"

# VENDOR_MK_ROOT
VENDOR_MK_ROOT_INTERNAL="${ANDROID_ROOT}"/vendor/"${VENDOR}"
VENDOR_MK_ROOT="${ANDROID_ROOT}"/vendor/"${VENDOR}"/"${DEVICE_COMMON}"

# BLOB_ROOT
BLOB_ROOT="${VENDOR_MK_ROOT}"/proprietary


if [ ! -f "${HELPER}" ]; then
    echo "Unable to find helper script at ${HELPER}"
    exit 1
fi

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

generate_prop_files_array "${MY_DIR}/${TOOLS_DIR}"

for codename in "${!PROP_CODENAMES[@]}"; do
    base_dir="${MY_DIR}/${TOOLS_DIR}/"
    codename_dir="${base_dir}/${codename}"
    generate_prop_files_array "$codename_dir"
done





    # Neverallow product copy files
    #local libraries=(
    #    libaudior7870
    #    libLifevibes_lvverx
    #    libLifevibes_lvvetx
    #    libpreprocessing_nxp
    #    librecordalive
    #    libtfa98xx
    #    libsamsungDiamondVoice
    #    libSamsungPostProcessConvertor
    #    lib_SamsungRec_06004
    #    lib_SamsungRec_06006
    #    libsecaudioinfo
    #    lib_soundaliveresampler
    #    lib_SoundAlive_SRC384_ver320
    #    libalsa7870
    #    audio.primary.exynos7870
    #    libGLES_mali
    #    Tfa9896.cnt
    #    libvndsecril-client
    #    libskeymaster3device
    #    libkeymaster_helper_vendor
    #    libkeymaster2_mdfpp
    #    keystore.mdfpp
    #)


# common helper
# source "${HELPER}"

for PROP_FILE in "${!PROP_FILES[@]}"; do
    # SOURCE_DIR=${PROP_FILES[$PROP_FILE]}

    COMMON_NAME="${INTERNAL_DEVICE_COMMON[$PROP_FILE]}"
    if [ -z "$COMMON_NAME" ]; then
    COMMON_NAME="dummy"
    fi

    # Warning headers and guards
    
    # helper needs to be in loop too to always get relauched with correct options
    source "${HELPER}"
    
    # setup_vendor "${DEVICE_COMMON}" "${VENDOR}" "${ANDROID_ROOT}" true

    setup_vendor "${COMMON_NAME}" "${VENDOR}" "${ANDROID_ROOT}" true
    
    if [[ "$PROP_FILE" != proprietary-files_*_audio.txt || "$PROP_FILE" != proprietary-files_a6lte.txt ]]; then
    write_headers "a3y17lte j5y17lte a6lte j6lte j7velte j7xelte j7y17lte on7xelte m10lte j7popelteskt"
    fi

    if [[ "$PROP_FILE" == "proprietary-files_m10lte_radio.txt" || "$PROP_FILE" == "proprietary-files_a6lte_audio.txt" ]]; then
    write_headers "a3y17lte j5y17lte a6lte j6lte j7velte j7xelte j7y17lte on7xelte m10lte j7popelteskt"
    fi

    if [[ "$PROP_FILE" == "proprietary-files_oss-prebuilt.txt" ]]; then
    write_headers "a3y17lte j5y17lte a6lte j6lte j7velte j7xelte j7y17lte on7xelte m10lte j7popelteskt"
    fi
    
    if [[ "$PROP_FILE" == "proprietary-files_m10lte_audio.txt" ]]; then
    write_headers "a3y17lte j5y17lte j6lte j7y17lte m10lte gtaxlwifi gtaxllte"
    fi
    
    if [[ "$PROP_FILE" == "proprietary-files_a6lte_audio.txt" ]]; then
    write_headers "a6lte j7velte j7xelte on7xelte j7popelteskt"
    fi

    if [[ "$PROP_FILE" == proprietary-files_gracerlte_bsp_p.txt ]]; then
    write_makefiles "${MY_DIR}/${TOOLS_DIR}/gracerlte/${PROP_FILE}"
    write_footers
    fi

    if [[ "$PROP_FILE" == proprietary-files_oss_hwc.txt ]]; then
    write_makefiles "${MY_DIR}/${TOOLS_DIR}/oss/${PROP_FILE}"
    write_footers
    fi

    if [[ "$PROP_FILE" == proprietary-files_m10lte*.txt ]]; then
    write_makefiles "${MY_DIR}/${TOOLS_DIR}/m10lte/${PROP_FILE}" true
    write_footers
    fi
    if [[ "$PROP_FILE" == proprietary-files_starlte*.txt ]]; then
    write_makefiles "${MY_DIR}/${TOOLS_DIR}/starlte/${PROP_FILE}"
    write_footers
    fi
    if [[ "$PROP_FILE" == proprietary-files_a6ltep*.txt ]]; then
    if [[ "$PROP_FILE" != proprietary-files_a6ltep.txt ]]; then
    write_makefiles "${MY_DIR}/${TOOLS_DIR}/a6ltep/${PROP_FILE}"
    write_footers
    fi
    fi
    if [[ "$PROP_FILE" == proprietary-files_a6lte*.txt ]]; then
    if [[ "$PROP_FILE" != proprietary-files_a6lte.txt ]]; then
    if [[ "$PROP_FILE" != proprietary-files_a6ltep_bsp_p.txt ]]; then
    if [[ "$PROP_FILE" != proprietary-files_a6ltep.txt ]]; then
    if [[ "$PROP_FILE" != proprietary-files_a6lte_p.txt ]]; then
    write_makefiles "${MY_DIR}/${TOOLS_DIR}/a6lte/${PROP_FILE}"
    write_footers
    fi
    fi
    fi
    fi
    fi
    if [[ "$PROP_FILE" == proprietary-files_a7y17lte*.txt ]]; then
    write_makefiles "${MY_DIR}/${TOOLS_DIR}/a7y17lte/${PROP_FILE}"
    write_footers
    fi
    

    #if [[ "${PROP_FILE}" == "proprietary-files_m10lte_audio.txt" ]]; then
    #   echo '# m10lte audio hal' >> "$VENDOR_MK_ROOT_AUDIO_M10LTE/${DEVICE_COMMON_AUDIO_M10LTE}-vendor.mk"
    #   echo 'ifeq ($(TARGET_DEVICE_HAS_M10LTE_AUDIO_HAL),true)' >> "$VENDOR_MK_ROOT_AUDIO_M10LTE/${DEVICE_COMMON_AUDIO_M10LTE}-vendor.mk"
    #fi
    
done

# cp -r ${INTERNAL_VENDOR_MK_ROOT_STARLTE} ${VENDOR_MK_ROOT}

DEVICE_COMMON_RADIO="sec_radio" #m10lte_radio #starlte_radio
DEVICE_COMMON_GNSS="sec_gnss" #a6lte_gnss
DEVICE_COMMON_TEE="tee" #a6lte_tee
DEVICE_COMMON_SECAPP="secapp" #a6lte_secapp
DEVICE_COMMON_SAMSUNG_SLSI="samsung_slsi" #a7y17lte_bsp
DEVICE_COMMON_SAMSUNG_SLSI_OMX="samsung_slsi_omx" #gracerlte_slsi_p
DEVICE_COMMON_SAMSUNG_SLSI_OSS="samsung_slsi_oss" #samsung_slsi_oss
DEVICE_COMMON_SAMSUNG_SLSI_Q="samsung_slsi_q" #a6lte_q_bsp
DEVICE_COMMON_SAMSUNG_SLSI_P="samsung_slsi_p" #a6lte_p_bsp
DEVICE_COMMON_KEYMASTER="sec_keymaster" #a6lte_keymaster
DEVICE_COMMON_TFA_SEC_AUDIO="tfa_sec_audio" #m10lte_audio
DEVICE_COMMON_SEC_AUDIO="sec_audio" #a6lte_audio
DEVICE_COMMON_GATEKEEPER_BIOMETRICS="gatekeeper-biometrics" #a6lte_gatekeeper
DEVICE_COMMON_GATEKEEPER="gatekeeper" #m10lte_gatekeeper


for key in "${!INTERNAL_DEVICE_COMMON[@]}"; do
    COMMON_NAME="${INTERNAL_DEVICE_COMMON[$key]}"
    VENDOR_MK_ROOT_INTERNAL_COMMON="${ANDROID_ROOT}"/vendor/"${VENDOR}"/"${COMMON_NAME}"
    
    mk_root_varname="INTERNAL_VENDOR_MK_ROOT_${COMMON_NAME}"
    blob_root_varname="BLOB_ROOT_${COMMON_NAME}"

    # patch every internal device-vendor.mk
    echo "${VENDOR_MK_ROOT_INTERNAL_COMMON}"
    #sed -i "s|${DEVICE_COMMON}|${DEVICE_COMMON}/${COMMON_NAME}|g" "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk"
    #sed -i "s|${VENDOR}/${COMMON_NAME}|${VENDOR}/${DEVICE_COMMON}/${COMMON_NAME}|g" "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk"
    sed -i "s|${DEVICE_COMMON}|${DEVICE_COMMON}/${COMMON_NAME}|g" "${VENDOR_MK_ROOT_INTERNAL_COMMON}"/*.mk
    sed -i "s|${VENDOR}/${COMMON_NAME}|${VENDOR}/${DEVICE_COMMON}/${COMMON_NAME}|g" "${VENDOR_MK_ROOT_INTERNAL_COMMON}"/*.mk
    
    # Using indirect variable reference to get the values
    #echo "$mk_root_varname:"
    #echo "${!mk_root_varname}"
    #echo "$blob_root_varname:"
    #echo "${!mk_root_varname}/proprietary"
    
    if [[ "$COMMON_NAME" == *_radio ]]; then
        sed -i "s|${COMMON_NAME}|${DEVICE_COMMON_RADIO}|g" "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk"
        mkdir -p "${VENDOR_MK_ROOT}/${DEVICE_COMMON_RADIO}"/proprietary
        cp -r "${VENDOR_MK_ROOT_INTERNAL_COMMON}"/proprietary "${VENDOR_MK_ROOT}/${DEVICE_COMMON_RADIO}"

        # OUR order
        #a6lte_keymaster
        #a6lte_gatekeeper
        #m10lte_gatekeeper
        #common
        #starlte
        #a7y17lte_secapp
        #a6lte
        #m10lte_radio
        #a6lte_secapp
        #a7y17lte_bsp
        #m10lte_audio
        #a7y17lte
        #m10lte
        #a6lte_tee
        #starlte_radio
        #a6lte_audio
        #a6lte_gnss

        if [[ "$COMMON_NAME" == m10lte_radio ]]; then
            cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_RADIO}/Android.mk"
            cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/BoardConfigVendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_RADIO}/BoardConfigVendor.mk"
            cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.bp" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_RADIO}/Android.bp"
            cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_RADIO}/${DEVICE_COMMON_RADIO}-vendor.mk"
        fi

        if [[ "$COMMON_NAME" == starlte_radio ]]; then
            sed -i '1,6d' "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk"
            sed -i '1,10d' "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.mk"
            sed -i '1,6d' "${VENDOR_MK_ROOT_INTERNAL_COMMON}/BoardConfigVendor.mk"
            sed -i '1,6d' "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.bp"

            cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_RADIO}/Android.mk"
            cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/BoardConfigVendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_RADIO}/BoardConfigVendor.mk"
            cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.bp" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_RADIO}/Android.bp"
            cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_RADIO}/${DEVICE_COMMON_RADIO}-vendor.mk"
            
            sed -i "/\b\(libvndsecril-client\)\b/d" "${VENDOR_MK_ROOT}/${DEVICE_COMMON_RADIO}/${DEVICE_COMMON_RADIO}-vendor.mk"
        fi
    fi

    if [[ "$COMMON_NAME" == a6lte_keymaster ]]; then
        sed -i "s|${COMMON_NAME}|${DEVICE_COMMON_KEYMASTER}|g" "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk"
        mkdir -p "${VENDOR_MK_ROOT}/${DEVICE_COMMON_KEYMASTER}"/proprietary
        cp -r "${VENDOR_MK_ROOT_INTERNAL_COMMON}"/proprietary "${VENDOR_MK_ROOT}/${DEVICE_COMMON_KEYMASTER}"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_KEYMASTER}/Android.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/BoardConfigVendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_KEYMASTER}/BoardConfigVendor.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.bp" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_KEYMASTER}/Android.bp"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_KEYMASTER}/${DEVICE_COMMON_KEYMASTER}-vendor.mk"
    fi

    if [[ "$COMMON_NAME" == a6lte_gatekeeper ]]; then
        sed -i "s|${COMMON_NAME}|${DEVICE_COMMON_GATEKEEPER_BIOMETRICS}|g" "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk"
        mkdir -p "${VENDOR_MK_ROOT}/${DEVICE_COMMON_GATEKEEPER_BIOMETRICS}"/proprietary
        cp -r "${VENDOR_MK_ROOT_INTERNAL_COMMON}"/proprietary "${VENDOR_MK_ROOT}/${DEVICE_COMMON_GATEKEEPER_BIOMETRICS}"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_GATEKEEPER_BIOMETRICS}/Android.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/BoardConfigVendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_GATEKEEPER_BIOMETRICS}/BoardConfigVendor.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.bp" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_GATEKEEPER_BIOMETRICS}/Android.bp"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_GATEKEEPER_BIOMETRICS}/${DEVICE_COMMON_GATEKEEPER_BIOMETRICS}-vendor.mk"
    fi

    if [[ "$COMMON_NAME" == m10lte_gatekeeper ]]; then
        sed -i "s|${COMMON_NAME}|${DEVICE_COMMON_GATEKEEPER}|g" "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk"
        mkdir -p "${VENDOR_MK_ROOT}/${DEVICE_COMMON_GATEKEEPER}"/proprietary
        cp -r "${VENDOR_MK_ROOT_INTERNAL_COMMON}"/proprietary "${VENDOR_MK_ROOT}/${DEVICE_COMMON_GATEKEEPER}"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_GATEKEEPER}/Android.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/BoardConfigVendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_GATEKEEPER}/BoardConfigVendor.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.bp" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_GATEKEEPER}/Android.bp"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_GATEKEEPER}/${DEVICE_COMMON_GATEKEEPER}-vendor.mk"
    fi

    if [[ "$COMMON_NAME" == m10lte_audio ]]; then
        sed -i "s|${COMMON_NAME}|${DEVICE_COMMON_TFA_SEC_AUDIO}|g" "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk"
        mkdir -p "${VENDOR_MK_ROOT}/${DEVICE_COMMON_TFA_SEC_AUDIO}"/proprietary
        cp -r "${VENDOR_MK_ROOT_INTERNAL_COMMON}"/proprietary "${VENDOR_MK_ROOT}/${DEVICE_COMMON_TFA_SEC_AUDIO}"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_TFA_SEC_AUDIO}/Android.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/BoardConfigVendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_TFA_SEC_AUDIO}/BoardConfigVendor.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.bp" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_TFA_SEC_AUDIO}/Android.bp"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_TFA_SEC_AUDIO}/${DEVICE_COMMON_TFA_SEC_AUDIO}-vendor.mk"

        # remove mali from copy files
sed -i "/\b\(libLifevibes_lvverx\|libLifevibes_lvvetx\|libpreprocessing_nxp\|librecordalive\|libsamsungDiamondVoice\|libSamsungPostProcessConvertor\|lib_SamsungRec_06006\|libsecaudioinfo\|lib_soundaliveresampler\|lib_SoundAlive_SRC384_ver320\|audio.primary.exynos7870\|libaudior7870\|libalsa7870\|libtfa98xx\)\b/d" \
"${VENDOR_MK_ROOT}/${DEVICE_COMMON_TFA_SEC_AUDIO}/${DEVICE_COMMON_TFA_SEC_AUDIO}-vendor.mk"
    fi

    if [[ "$COMMON_NAME" == a6lte_audio ]]; then
        sed -i "s|${COMMON_NAME}|${DEVICE_COMMON_SEC_AUDIO}|g" "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk"
        mkdir -p "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SEC_AUDIO}"/proprietary
        cp -r "${VENDOR_MK_ROOT_INTERNAL_COMMON}"/proprietary "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SEC_AUDIO}"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SEC_AUDIO}/Android.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/BoardConfigVendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SEC_AUDIO}/BoardConfigVendor.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.bp" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SEC_AUDIO}/Android.bp"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SEC_AUDIO}/${DEVICE_COMMON_SEC_AUDIO}-vendor.mk"
        sed -i "/\b\(libLifevibes_lvverx\|libLifevibes_lvvetx\|libpreprocessing_nxp\|librecordalive\|libsamsungDiamondVoice\|libSamsungPostProcessConvertor\|lib_SamsungRec_06004\|libsecaudioinfo\|lib_soundaliveresampler\|lib_SoundAlive_SRC384_ver320\|audio.primary.exynos7870\|libaudior7870\|libalsa7870\)\b/d" \
"${VENDOR_MK_ROOT}/${DEVICE_COMMON_SEC_AUDIO}/${DEVICE_COMMON_SEC_AUDIO}-vendor.mk"
    fi

    if [[ "$COMMON_NAME" == a6lte_gnss ]]; then
        sed -i "s|${COMMON_NAME}|${DEVICE_COMMON_GNSS}|g" "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk"
        mkdir -p "${VENDOR_MK_ROOT}/${DEVICE_COMMON_GNSS}"/proprietary
        cp -r "${VENDOR_MK_ROOT_INTERNAL_COMMON}"/proprietary "${VENDOR_MK_ROOT}/${DEVICE_COMMON_GNSS}"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_GNSS}/Android.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/BoardConfigVendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_GNSS}/BoardConfigVendor.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.bp" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_GNSS}/Android.bp"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_GNSS}/${DEVICE_COMMON_GNSS}-vendor.mk"
    fi

    if [[ "$COMMON_NAME" == a6lte_tee ]]; then
        sed -i "s|${COMMON_NAME}|${DEVICE_COMMON_TEE}|g" "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk"
        mkdir -p "${VENDOR_MK_ROOT}/${DEVICE_COMMON_TEE}"/proprietary
        cp -r "${VENDOR_MK_ROOT_INTERNAL_COMMON}"/proprietary "${VENDOR_MK_ROOT}/${DEVICE_COMMON_TEE}"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_TEE}/Android.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/BoardConfigVendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_TEE}/BoardConfigVendor.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.bp" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_TEE}/Android.bp"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_TEE}/${DEVICE_COMMON_TEE}-vendor.mk"
    fi

    if [[ "$COMMON_NAME" == a6lte_secapp ]]; then
        sed -i "s|${COMMON_NAME}|${DEVICE_COMMON_SECAPP}|g" "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk"
        mkdir -p "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SECAPP}"/proprietary
        cp -r "${VENDOR_MK_ROOT_INTERNAL_COMMON}"/proprietary "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SECAPP}"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SECAPP}/Android.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/BoardConfigVendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SECAPP}/BoardConfigVendor.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.bp" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SECAPP}/Android.bp"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SECAPP}/${DEVICE_COMMON_SECAPP}-vendor.mk"
    fi

    if [[ "$COMMON_NAME" == a7y17lte_bsp ]]; then
        sed -i "s|${COMMON_NAME}|${DEVICE_COMMON_SAMSUNG_SLSI}|g" "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk"
        mkdir -p "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI}"/proprietary
        cp -r "${VENDOR_MK_ROOT_INTERNAL_COMMON}"/proprietary "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI}"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI}/Android.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/BoardConfigVendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI}/BoardConfigVendor.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.bp" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI}/Android.bp"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI}/${DEVICE_COMMON_SAMSUNG_SLSI}-vendor.mk"
    fi

    if [[ "$COMMON_NAME" == oss_hwc ]]; then
        sed -i "s|${COMMON_NAME}|${DEVICE_COMMON_SAMSUNG_SLSI_OSS}|g" "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk"
        mkdir -p "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_OSS}"/proprietary
        cp -r "${VENDOR_MK_ROOT_INTERNAL_COMMON}"/proprietary "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_OSS}"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_OSS}/Android.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/BoardConfigVendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_OSS}/BoardConfigVendor.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.bp" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_OSS}/Android.bp"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_OSS}/${DEVICE_COMMON_SAMSUNG_SLSI_OSS}-vendor.mk"
    fi

    if [[ "$COMMON_NAME" == gracerlte_bsp_p ]]; then
        sed -i "s|${COMMON_NAME}|${DEVICE_COMMON_SAMSUNG_SLSI_OMX}|g" "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk"
        mkdir -p "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_OMX}"/proprietary
        cp -r "${VENDOR_MK_ROOT_INTERNAL_COMMON}"/proprietary "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_OMX}"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_OMX}/Android.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/BoardConfigVendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_OMX}/BoardConfigVendor.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.bp" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_OMX}/Android.bp"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_OMX}/${DEVICE_COMMON_SAMSUNG_SLSI_OMX}-vendor.mk"
    fi

    if [[ "$COMMON_NAME" == a6ltep_bsp_p ]]; then
        sed -i "s|${COMMON_NAME}|${DEVICE_COMMON_SAMSUNG_SLSI_P}|g" "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk"
        mkdir -p "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_P}"/proprietary
        cp -r "${VENDOR_MK_ROOT_INTERNAL_COMMON}"/proprietary "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_P}"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_P}/Android.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/BoardConfigVendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_P}/BoardConfigVendor.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.bp" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_P}/Android.bp"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_P}/${DEVICE_COMMON_SAMSUNG_SLSI_P}-vendor.mk"
    fi

    #if [[ "$COMMON_NAME" == a6lte_bsp ]]; then
    #    sed -i "s|${COMMON_NAME}|${DEVICE_COMMON_SAMSUNG_SLSI_Q}|g" "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk"
    #    mkdir -p "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_Q}"/proprietary
    #    cp -r "${VENDOR_MK_ROOT_INTERNAL_COMMON}"/proprietary "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_Q}"
    #    cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_Q}/Android.mk"
    #    cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/BoardConfigVendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_Q}/BoardConfigVendor.mk"
    #    cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.bp" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_Q}/Android.bp"
    #    cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON_SAMSUNG_SLSI_Q}/${DEVICE_COMMON_SAMSUNG_SLSI_Q}-vendor.mk"
    #fi

    if [[ "$COMMON_NAME" == m10lte || "$COMMON_NAME" == starlte || "$COMMON_NAME" == a7y17lte ]]; then
        mkdir -p "${VENDOR_MK_ROOT}"/proprietary
        cp -r "${VENDOR_MK_ROOT_INTERNAL_COMMON}"/proprietary "${VENDOR_MK_ROOT}"

        # common
        sed -i "s|${DEVICE_COMMON}/${COMMON_NAME}|${DEVICE_COMMON}|g" "${VENDOR_MK_ROOT_INTERNAL_COMMON}"/*.mk

        if [[ "$COMMON_NAME" != starlte ]]; then
        sed -i '1,6d' "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk"
        sed -i '1,10d' "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.mk"
        sed -i '1,6d' "${VENDOR_MK_ROOT_INTERNAL_COMMON}/BoardConfigVendor.mk"
        sed -i '1,6d' "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.bp"
        fi

        #mkdir -p "${VENDOR_MK_ROOT}"
        #cp -r "${VENDOR_MK_ROOT_INTERNAL_COMMON}" "${VENDOR_MK_ROOT}"
        touch "${VENDOR_MK_ROOT}/${DEVICE_COMMON}-vendor.mk"
        touch "${VENDOR_MK_ROOT}/Android.mk"
        touch "${VENDOR_MK_ROOT}/BoardConfigVendor.mk"
        touch "${VENDOR_MK_ROOT}/Android.bp"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.mk" >> "${VENDOR_MK_ROOT}/Android.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/BoardConfigVendor.mk" >> "${VENDOR_MK_ROOT}/BoardConfigVendor.mk"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/Android.bp" >> "${VENDOR_MK_ROOT}/Android.bp"
        cat "${VENDOR_MK_ROOT_INTERNAL_COMMON}/${COMMON_NAME}-vendor.mk" >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON}-vendor.mk"
    fi


    echo "$COMMON_NAME"

#a6lte_keymaster
#a6lte_gatekeeper
#m10lte_gatekeeper
#common
#starlte
#a7y17lte_secapp
#a6lte
#hello radio
#m10lte_radio
#a6lte_secapp
#a7y17lte_bsp
#m10lte_audio
#a7y17lte
#m10lte
#a6lte_tee
#hello radio
#starlte_radio
#a6lte_audio
#a6lte_gnss

done

# remove mali from copy files
sed -i "/\b\(libGLES_mali\)\b/d" "${VENDOR_MK_ROOT}/${DEVICE_COMMON}-vendor.mk"
sed -i "/\b\(endif\)\b/d" "${VENDOR_MK_ROOT}/Android.mk"

############################################################################################################
# CUSTOM PART START (Taken from https://github.com/LineageOS/android_device_samsung_universal7580-common)  #
############################################################################################################
(cat << EOF) >> "${VENDOR_MK_ROOT}/${DEVICE_COMMON}-vendor.mk"

# secapp
-include vendor/samsung/${DEVICE_COMMON}/${DEVICE_COMMON_SECAPP}/${DEVICE_COMMON_SECAPP}-vendor.mk

# teegris
-include vendor/samsung/${DEVICE_COMMON}/${DEVICE_COMMON_TEE}/${DEVICE_COMMON_TEE}-vendor.mk

# gatekeeper
ifeq (\$(TARGET_DEVICE_HAS_HW_GATEKEEPER_BIOMETRICS),true)
-include vendor/samsung/${DEVICE_COMMON}/${DEVICE_COMMON_GATEKEEPER_BIOMETRICS}/${DEVICE_COMMON_GATEKEEPER_BIOMETRICS}-vendor.mk
endif

ifeq (\$(TARGET_DEVICE_HAS_HW_GATEKEEPER_COMMON),true)
-include vendor/samsung/${DEVICE_COMMON}/${DEVICE_COMMON_GATEKEEPER}/${DEVICE_COMMON_GATEKEEPER}-vendor.mk
endif

# radio
ifeq (\$(TARGET_DEVICE_HAS_SEC_RIL),true)
-include vendor/samsung/${DEVICE_COMMON}/${DEVICE_COMMON_RADIO}/${DEVICE_COMMON_RADIO}-vendor.mk
endif

# audio
ifeq (\$(TARGET_DEVICE_HAS_SEC_AUDIO),true)
-include vendor/samsung/${DEVICE_COMMON}/${DEVICE_COMMON_SEC_AUDIO}/${DEVICE_COMMON_SEC_AUDIO}-vendor.mk
endif

ifeq (\$(TARGET_DEVICE_HAS_TFA_SEC_AUDIO),true)
-include vendor/samsung/${DEVICE_COMMON}/${DEVICE_COMMON_TFA_SEC_AUDIO}/${DEVICE_COMMON_TFA_SEC_AUDIO}-vendor.mk
endif

# gnss
ifeq (\$(TARGET_DEVICE_HAS_SEC_GNSS),true)
-include vendor/samsung/${DEVICE_COMMON}/${DEVICE_COMMON_GNSS}/${DEVICE_COMMON_GNSS}-vendor.mk
endif

# misc
ifeq (\$(TARGET_DEVICE_HAS_SAMSUNG_SLSI_EXYNOS7870),true)
-include vendor/samsung/${DEVICE_COMMON}/${DEVICE_COMMON_SAMSUNG_SLSI}/${DEVICE_COMMON_SAMSUNG_SLSI}-vendor.mk
-include vendor/samsung/${DEVICE_COMMON}/${DEVICE_COMMON_SAMSUNG_SLSI_OMX}/${DEVICE_COMMON_SAMSUNG_SLSI_OMX}-vendor.mk
-include vendor/samsung/${DEVICE_COMMON}/${DEVICE_COMMON_SAMSUNG_SLSI_P}/${DEVICE_COMMON_SAMSUNG_SLSI_P}-vendor.mk
-include vendor/samsung/${DEVICE_COMMON}/${DEVICE_COMMON_SAMSUNG_SLSI_Q}/${DEVICE_COMMON_SAMSUNG_SLSI_Q}-vendor.mk
-include vendor/samsung/${DEVICE_COMMON}/${DEVICE_COMMON_SAMSUNG_SLSI_OSS}/${DEVICE_COMMON_SAMSUNG_SLSI_OSS}-vendor.mk
endif

# keymaster & keystore
ifeq (\$(TARGET_DEVICE_HAS_SEC_KEYMASTER),true)
-include vendor/samsung/${DEVICE_COMMON}/${DEVICE_COMMON_KEYMASTER}/${DEVICE_COMMON_KEYMASTER}-vendor.mk
endif
EOF


############################################################################################################
# CUSTOM PART START (Taken from https://github.com/LineageOS/android_device_samsung_universal7580-common)  #
############################################################################################################
(cat << EOF) >> "${VENDOR_MK_ROOT}/Android.mk"
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


ifeq (\$(TARGET_BOARD_HAS_SEC_AUDIO_HAL),true)
LOCAL_AUDIO_VARIANT_DIR := sec_audio
LOCAL_SAMSUNGREC_VARIANT := 06004
LOCAL_USE_STARLTE_VNDSECRIL := true
LOCAL_EXYNOS7870_AUDIO_GUARD := true
endif

ifeq (\$(TARGET_BOARD_HAS_TFA_SEC_AUDIO_HAL),true)
LOCAL_AUDIO_VARIANT_DIR := tfa_sec_audio
LOCAL_SAMSUNGREC_VARIANT := 06006
LOCAL_USE_STARLTE_VNDSECRIL := true
LOCAL_USE_TFA_AMP := true
LOCAL_EXYNOS7870_AUDIO_GUARD := true
endif

# TFA AUDIO shoud be avaiable when needed
ifeq (\$(TARGET_AUDIOHAL_VARIANT),samsung-linaro-exynos7870)
LOCAL_USE_TFA_AMP := true
LOCAL_AUDIO_VARIANT_DIR := tfa_sec_audio
LOCAL_USE_STARLTE_VNDSECRIL := true
endif
ifeq (\$(TARGET_AUDIOHAL_VARIANT),samsung-exynos7870)
LOCAL_USE_TFA_AMP := true
LOCAL_AUDIO_VARIANT_DIR := tfa_sec_audio
LOCAL_USE_STARLTE_VNDSECRIL := true
endif

ifeq (\$(LOCAL_USE_TFA_AMP),true)
include \$(CLEAR_VARS)
LOCAL_MODULE := libtfa98xx
LOCAL_MODULE_OWNER := $VENDOR
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := \$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/libtfa98xx.so
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
LOCAL_SRC_FILES_64 := sec_radio/proprietary/vendor/lib64/libvndsecril-client.so
LOCAL_SRC_FILES_32 := sec_radio/proprietary/vendor/lib/libvndsecril-client.so
LOCAL_MULTILIB := both
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := liblog libcutils libhardware_legacy libfloatingfeature libc++ libc libm libdl
include \$(BUILD_PREBUILT)
endif

ifeq (\$(LOCAL_EXYNOS7870_AUDIO_GUARD),true)

include \$(CLEAR_VARS)
LOCAL_MODULE := libLifevibes_lvverx
LOCAL_MODULE_OWNER := $VENDOR
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := \$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/libLifevibes_lvverx.so
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
LOCAL_SRC_FILES_32 := \$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/libLifevibes_lvvetx.so
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
LOCAL_SRC_FILES_32 := \$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/libpreprocessing_nxp.so
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
LOCAL_SRC_FILES_32 := \$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/librecordalive.so
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
LOCAL_MODULE_OWNER := $VENDOR
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := \$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/libsamsungDiamondVoice.so
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
LOCAL_SRC_FILES_32 := \$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/libSamsungPostProcessConvertor.so
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
LOCAL_SRC_FILES_32 := \$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/lib_SamsungRec_\$(LOCAL_SAMSUNGREC_VARIANT).so
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
LOCAL_MODULE_OWNER := $VENDOR
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := \$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/libsecaudioinfo.so
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
LOCAL_SRC_FILES_32 := \$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/lib_soundaliveresampler.so
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
LOCAL_SRC_FILES_32 := \$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/lib_SoundAlive_SRC384_ver320.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := libc libdl liblog libm
include \$(BUILD_PREBUILT)


include \$(CLEAR_VARS)
LOCAL_MODULE := audio.primary.exynos7870
LOCAL_MODULE_OWNER := $VENDOR
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := \$(LOCAL_AUDIO_VARIANT_DIR)/proprietary/vendor/lib/hw/audio.primary.exynos7870.so
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
endif

EOF

(cat << EOF) >> "${VENDOR_MK_ROOT}/$DEVICE_COMMON-vendor.mk"

# Create Mali links for Vulkan and OpenCL
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
    vendor/samsung/universal7870-common/tfa_sec_audio/proprietary/vendor/etc/Tfa9896.cnt:\$(TARGET_COPY_OUT_VENDOR)/etc/Tfa9896.cnt
endif

EOF


# cleanup
for key in "${!INTERNAL_DEVICE_COMMON[@]}"; do
    COMMON_NAME="${INTERNAL_DEVICE_COMMON[$key]}"
    VENDOR_MK_ROOT_INTERNAL_COMMON="${ANDROID_ROOT}"/vendor/"${VENDOR}"/"${COMMON_NAME}"

    # patch every internal device-vendor.mk
    echo "${VENDOR_MK_ROOT_INTERNAL_COMMON}"

rm -rf "${VENDOR_MK_ROOT_INTERNAL_COMMON}"

done

rm -rf $ANDROID_ROOT/device/$VENDOR/$DEVICE_COMMON/$TOOLS_DIR/a6lte
rm -rf $ANDROID_ROOT/device/$VENDOR/$DEVICE_COMMON/$TOOLS_DIR/a6ltep
rm -rf $ANDROID_ROOT/device/$VENDOR/$DEVICE_COMMON/$TOOLS_DIR/m10lte
rm -rf $ANDROID_ROOT/device/$VENDOR/$DEVICE_COMMON/$TOOLS_DIR/starlte
rm -rf $ANDROID_ROOT/device/$VENDOR/$DEVICE_COMMON/$TOOLS_DIR/a7y17lte

