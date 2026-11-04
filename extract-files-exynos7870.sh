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
#proprietary-files_P_a7y17lte_mali.txt
#proprietary-files_P_a7y17lte_samsung-slsi.txt
#proprietary-files_P_gracerlte_samsung-slsi.txt
#proprietary-files_Q_a6lte_audio.txt
#proprietary-files_Q_a6lte_drm.txt
#proprietary-files_Q_a6lte_gnss.txt
#proprietary-files_Q_a6lte_secapp.txt
#proprietary-files_Q_m10lte_audio.txt
#proprietary-files_Q_m10lte_media.txt
#proprietary-files_Q_m10lte_radio.txt
#proprietary-files_Q_m10lte_sensors.txt
#proprietary-files_Q_j7duolte_radio.txt
#proprietary-files_R_oss_hwc.txt


# Audio files
EXTRACT_UTILS_ONLY_LIST=proprietary-files_Q_a6lte_audio.txt ./extract-files.py -n -k $COMMON_Q_A6LTE_PATH
EXTRACT_UTILS_ONLY_LIST=proprietary-files_Q_m10lte_audio.txt ./extract-files.py -n -k $COMMON_Q_M10LTE_PATH

# GNSS files
EXTRACT_UTILS_ONLY_LIST=proprietary-files_Q_a6lte_gnss.txt ./extract-files.py -n -k $COMMON_Q_A6LTE_PATH

# Secapp files
EXTRACT_UTILS_ONLY_LIST=proprietary-files_Q_a6lte_secapp.txt ./extract-files.py -n -k $COMMON_Q_A6LTE_PATH

# Radio files
EXTRACT_UTILS_ONLY_LIST=proprietary-files_Q_m10lte_radio.txt ./extract-files.py -n -k $COMMON_Q_M10LTE_PATH
EXTRACT_UTILS_ONLY_LIST=proprietary-files_Q_j7duolte_radio.txt ./extract-files.py -n -k $COMMON_Q_J7DUOLTE_PATH

# Sensors files
EXTRACT_UTILS_ONLY_LIST=proprietary-files_Q_m10lte_sensors.txt ./extract-files.py -n -k $COMMON_Q_M10LTE_PATH

# Media files
EXTRACT_UTILS_ONLY_LIST=proprietary-files_Q_m10lte_media.txt ./extract-files.py -n -k $COMMON_Q_M10LTE_PATH

# DRM files (Widevine 1.2 + Samsung key provisioning, same firmware as the
# TEE trustlets in the secapp list)
EXTRACT_UTILS_ONLY_LIST=proprietary-files_Q_a6lte_drm.txt ./extract-files.py -n -k $COMMON_Q_A6LTE_PATH

# Mali files
EXTRACT_UTILS_ONLY_LIST=proprietary-files_P_a7y17lte_mali.txt ./extract-files.py -n -k $COMMON_P_A7Y17LTE_PATH

# Samsung SLSI files
EXTRACT_UTILS_ONLY_LIST=proprietary-files_P_a6lte_samsung-slsi.txt ./extract-files.py -n -k $COMMON_P_A6LTE_PATH
EXTRACT_UTILS_ONLY_LIST=proprietary-files_P_a7y17lte_samsung-slsi.txt ./extract-files.py -n -k $COMMON_P_A7Y17LTE_PATH
EXTRACT_UTILS_ONLY_LIST=proprietary-files_P_gracerlte_samsung-slsi.txt ./extract-files.py -n -k $COMMON_P_GRACERLTE_PATH

# HWC files (OSS - Android R)
EXTRACT_UTILS_ONLY_LIST=proprietary-files_R_oss_hwc.txt ./extract-files.py -n -k $COMMON_R_OSS_PATH
