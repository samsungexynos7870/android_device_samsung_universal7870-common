#
# This policy configuration will be used by samsung products
#

SYSTEM_EXT_PUBLIC_SEPOLICY_DIRS += \
    device/samsung/universal7870-common/sepolicy_slsi_exynos7870/common/public

SYSTEM_EXT_PRIVATE_SEPOLICY_DIRS += \
    device/samsung/universal7870-common/sepolicy_slsi_exynos7870/common/private

BOARD_VENDOR_SEPOLICY_DIRS += \
    device/samsung/universal7870-common/sepolicy_slsi_exynos7870/common/vendor

BOARD_VENDOR_SEPOLICY_DIRS += \
    device/samsung/universal7870-common/sepolicy_slsi_exynos7870/tee/mobicore/legacy

BOARD_VENDOR_SEPOLICY_DIRS += \
    device/samsung/universal7870-common/sepolicy_slsi_exynos7870/tee/mobicore/common
