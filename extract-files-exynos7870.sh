MY_DIR="${BASH_SOURCE%/*}"
if [[ ! -d "${MY_DIR}" ]]; then MY_DIR="${PWD}"; fi

ORIGINAL_DIR="${PWD}"

ANDROID_ROOT="${MY_DIR}/../../.."
DEVICE_COMMON=universal7870-common
VENDOR=samsung
VENDOR_MK_ROOT="${ANDROID_ROOT}"/vendor/"${VENDOR}"
DEVICE_COMMON_ROOT="${ANDROID_ROOT}"/device/"${VENDOR}"/"${DEVICE_COMMON}"

TARGET_SOURCES_DIR="${VENDOR_MK_ROOT}/tmp/sources"
mkdir -p "$TARGET_SOURCES_DIR"

REPO_URLS=(
    "https://github.com/Exynos7870-labs/samsung_gracerltektt_dump.git -b gracerltektt-user-9-PPR1.180610.011-N935KKKU4CVG1-release-keys N935KKKU4CVG1"
    "https://github.com/Exynos7870-labs/samsung_a6lte_dump.git -b a6ltejx-user-10-QP1A.190711.020-A600FJXU9CVB1-release-keys A600FJXU9CVB1"
    "https://github.com/Exynos7870-labs/samsung_a6lte_dump.git -b a6ltexx-user-9-PPR1.180610.011-A600FNXXS5BTC2-release-keys A600FNXXS5BTC2"
    "https://github.com/Exynos7870-labs/samsung_m10lte_dump.git -b m10ltedx-user-10-QP1A.190711.020-M105GDXSACWA1-release-keys M105GDXSACWA1"
    "https://github.com/Exynos7870-labs/samsung_j7duolte_dump.git -b j7duoltedd-user-10-QP1A.190711.020-J720FDDS7CUL1-release-keys J720FDDS7CUL1"
    "https://github.com/Exynos7870-labs/samsung_a7y17lteskt_dump.git -b a7y17lteskt-user-9-PPR1.180610.011-A720SKSU5CUJ2-release-keys A720SKSU5CUJ2"
    "https://github.com/Exynos7870-labs/samsung_slsi_oss.git -b lineage-18.1_17-11-2025 lineage-18_17-11-2025"
)

cd "$TARGET_SOURCES_DIR"
for i in "${!REPO_URLS[@]}"; do
    # Extract target directory name (last word in the string)
    repo_info="${REPO_URLS[$i]}"
    target_dir="${repo_info##* }"
    
    echo "Checking: $target_dir"
    
    if [[ -d "$target_dir" ]]; then
        echo "  Directory $target_dir already exists. Skipping."
    else
        echo "  Cloning: ${REPO_URLS[$i]}"
        git clone ${REPO_URLS[$i]}
    fi
done
ls
cd "$ORIGINAL_DIR"

COMMON_Q_A6LTE_PATH="${TARGET_SOURCES_DIR}/A600FJXU9CVB1"
COMMON_P_A6LTE_PATH="${TARGET_SOURCES_DIR}/A600FNXXS5BTC2"
COMMON_P_A7Y17LTE_PATH="${TARGET_SOURCES_DIR}/A720SKSU5CUJ2"
COMMON_Q_M10LTE_PATH="${TARGET_SOURCES_DIR}/M105GDXSACWA1"
COMMON_Q_J7DUOLTE_PATH="${TARGET_SOURCES_DIR}/J720FDDS7CUL1"
COMMON_P_GRACERLTE_PATH="${TARGET_SOURCES_DIR}/N935KKKU4CVG1"
COMMON_R_OSS_PATH="${TARGET_SOURCES_DIR}/lineage-18_17-11-2025"

#proprietary-files_P_a6lte_samsung-slsi.txt
#proprietary-files_P_a7y17lte_drm.txt
#proprietary-files_P_a7y17lte_mali.txt
#proprietary-files_P_a7y17lte_samsung-slsi.txt
#proprietary-files_P_gracerlte_samsung-slsi.txt
#proprietary-files_Q_a6lte_audio.txt
#proprietary-files_Q_a6lte_gatekeeper.txt
#proprietary-files_Q_a6lte_gnss.txt
#proprietary-files_Q_a6lte_keymaster.txt
#proprietary-files_Q_a6lte_secapp.txt
#proprietary-files_Q_a6lte_tee.txt
#proprietary-files_Q_m10lte_audio.txt
#proprietary-files_Q_m10lte_camera.txt
#proprietary-files_Q_m10lte_gatekeeper.txt
#proprietary-files_Q_m10lte_media.txt
#proprietary-files_Q_m10lte_radio.txt
#proprietary-files_Q_m10lte_sensors.txt
#proprietary-files_Q_j7duolte_radio.txt
#proprietary-files_R_oss_hwc.txt


# Camera files
./extract-files.sh universal7870-common/camera/Q vendor-tools/proprietary-files_Q_m10lte_camera.txt -n -k $COMMON_Q_M10LTE_PATH

# Audio files
./extract-files.sh universal7870-common/audio/sec vendor-tools/proprietary-files_Q_a6lte_audio.txt -n -k $COMMON_Q_A6LTE_PATH
./extract-files.sh universal7870-common/audio/sec_tfa vendor-tools/proprietary-files_Q_m10lte_audio.txt -n -k $COMMON_Q_M10LTE_PATH

# GNSS files
./extract-files.sh universal7870-common/gnss vendor-tools/proprietary-files_Q_a6lte_gnss.txt -n -k $COMMON_Q_A6LTE_PATH

# Keymaster files
./extract-files.sh universal7870-common/keymaster vendor-tools/proprietary-files_Q_a6lte_keymaster.txt -n -k $COMMON_Q_A6LTE_PATH

# Secapp files
./extract-files.sh universal7870-common/secapp vendor-tools/proprietary-files_Q_a6lte_secapp.txt -n -k $COMMON_Q_A6LTE_PATH

# Radio files
./extract-files.sh universal7870-common/radio vendor-tools/proprietary-files_Q_m10lte_radio.txt -n -k $COMMON_Q_M10LTE_PATH
./extract-files.sh universal7870-common/radio vendor-tools/proprietary-files_Q_j7duolte_radio.txt -n -k $COMMON_Q_J7DUOLTE_PATH

# Sensors files
./extract-files.sh universal7870-common/sensors vendor-tools/proprietary-files_Q_m10lte_sensors.txt -n -k $COMMON_Q_M10LTE_PATH

# Media files
./extract-files.sh universal7870-common/media vendor-tools/proprietary-files_Q_m10lte_media.txt -n -k $COMMON_Q_M10LTE_PATH

# DRM files
./extract-files.sh universal7870-common/drm vendor-tools/proprietary-files_P_a7y17lte_drm.txt -n -k $COMMON_P_A7Y17LTE_PATH

# Mali files
./extract-files.sh universal7870-common vendor-tools/proprietary-files_P_a7y17lte_mali.txt -n -k $COMMON_P_A7Y17LTE_PATH

# Samsung SLSI files
./extract-files.sh universal7870-common/samsung-slsi vendor-tools/proprietary-files_P_a6lte_samsung-slsi.txt -n -k $COMMON_P_A6LTE_PATH
./extract-files.sh universal7870-common/samsung-slsi vendor-tools/proprietary-files_P_a7y17lte_samsung-slsi.txt -n -k $COMMON_P_A7Y17LTE_PATH
./extract-files.sh universal7870-common/samsung-slsi vendor-tools/proprietary-files_P_gracerlte_samsung-slsi.txt -n -k $COMMON_P_GRACERLTE_PATH

# HWC files (OSS - Android R)
./extract-files.sh universal7870-common/samsung-slsi vendor-tools/proprietary-files_R_oss_hwc.txt -n -k $COMMON_R_OSS_PATH

# Gatekeeper files
./extract-files.sh universal7870-common/gatekeeper vendor-tools/proprietary-files_Q_m10lte_gatekeeper.txt -n -k $COMMON_Q_M10LTE_PATH
./extract-files.sh universal7870-common/gatekeeper-biometrics vendor-tools/proprietary-files_Q_a6lte_gatekeeper.txt -n -k $COMMON_Q_A6LTE_PATH

# TEE files
./extract-files.sh universal7870-common/tee vendor-tools/proprietary-files_Q_a6lte_tee.txt -n -k $COMMON_Q_A6LTE_PATH
