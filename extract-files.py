#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: 2017-2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

from extract_utils.fixups_blob import (
    blob_fixup,
    blob_fixups_user_type,
)
from extract_utils.main import (
    ExtractUtils,
    ExtractUtilsModule,
)
from extract_utils.module import (
    ProprietaryFile,
)

HWCOMPOSER_ORIG = b'sys/devices/\x0014830000.decon_f/vsync\x0014860000.sysmmu/14860000.sysmmu/\x00exynos5-fb.1/vsync\x00platform/exynos-sysmmu.30/exynos-sysmmu.11/\x00failed '
HWCOMPOSER_NEW = b'sys/devices/\x0014830000.decon_fb/vsync\x0014850000.sysmmu/14850000.sysmmu/\x00exynos5-fb.1/vsync\x00platform/exynos-sysmmu.30/exynos-sysmmu.11/\x00faild '

blob_fixups: blob_fixups_user_type = {
    'vendor/lib/hw/audio.primary.exynos7870.so': blob_fixup()
        .replace_needed('libtinyalsa.so', 'libalsa7870.so')
        .replace_needed('libaudioroute.so', 'libaudior7870.so'),
    ('vendor/lib/libpreprocessing_nxp.so', 'vendor/lib64/libpreprocessing_nxp.so'): blob_fixup()
        .replace_needed('libtinyalsa.so', 'libalsa7870.so'),
    'vendor/lib/lib_SamsungRec_06004.so': blob_fixup()
        .replace_needed('libstdc++.so', 'libc++.so'),
    ('vendor/lib/hw/memtrack.exynos7870.so', 'vendor/lib64/hw/memtrack.exynos7870.so'): blob_fixup()
        .binary_regex_replace(b'memtrack.universal7880.so', b'memtrack.universal7870.so'),
    ('vendor/lib/hw/hwcomposer.exynos7870.so', 'vendor/lib64/hw/hwcomposer.exynos7870.so'): blob_fixup()
        .binary_regex_replace(HWCOMPOSER_ORIG, HWCOMPOSER_NEW),
    'vendor/bin/hw/rild': blob_fixup()
        .replace_needed('libril.so', 'libril-samsung.so'),
    (
        'vendor/lib/libsec-ril.so',
        'vendor/lib64/libsec-ril.so',
        'vendor/lib/libsec-ril-dsds.so',
        'vendor/lib64/libsec-ril-dsds.so',
    ): blob_fixup()
        .replace_needed('libril.so', 'libril-samsung.so')
        .add_needed('libcutils_shim_vendor.so')
        .remove_needed('libnetutils.so'),
    'vendor/lib/libsensorlistener.so': blob_fixup()
        .add_needed('libshim_sensorndkbridge.so'),
    ('vendor/lib/libwvhidl.so', 'vendor/lib64/libwvhidl.so'): blob_fixup()
        .replace_needed('libprotobuf-cpp-lite.so', 'libprotobuf-cpp-lite-v29.so'),
    ('vendor/lib/mediadrm/libwvdrmengine.so', 'vendor/lib64/mediadrm/libwvdrmengine.so'): blob_fixup()
        .replace_needed('libprotobuf-cpp-lite.so', 'libprotobuf-cpp-lite-v29.so'),
    'vendor/etc/gnss/gps.cfg': blob_fixup()
        .regex_replace('Agnss_ShaVariant=0', 'Agnss_ShaVariant=256'),
}  # fmt: skip

module = ExtractUtilsModule(
    'universal7870-common',
    'samsung',
    blob_fixups=blob_fixups,
    skip_main_proprietary_file=True,
    proprietary_files=[
        ProprietaryFile('vendor-tools/proprietary-files_Q_a6lte_audio.txt', 'audio/sec'),
        ProprietaryFile('vendor-tools/proprietary-files_Q_m10lte_audio.txt', 'audio/sec_tfa'),
        ProprietaryFile('vendor-tools/proprietary-files_Q_a6lte_gnss.txt', 'gnss'),
        ProprietaryFile('vendor-tools/proprietary-files_Q_a6lte_secapp.txt', 'secapp'),
        ProprietaryFile('vendor-tools/proprietary-files_Q_m10lte_radio.txt', 'radio'),
        ProprietaryFile('vendor-tools/proprietary-files_Q_j7duolte_radio.txt', 'radio'),
        ProprietaryFile('vendor-tools/proprietary-files_Q_m10lte_sensors.txt', 'sensors'),
        ProprietaryFile('vendor-tools/proprietary-files_Q_m10lte_media.txt', 'media'),
        ProprietaryFile('vendor-tools/proprietary-files_Q_a6lte_drm.txt', 'drm'),
        ProprietaryFile('vendor-tools/proprietary-files_P_a7y17lte_mali.txt', ''),
        ProprietaryFile('vendor-tools/proprietary-files_P_a6lte_samsung-slsi.txt', 'samsung-slsi'),
        ProprietaryFile('vendor-tools/proprietary-files_P_a7y17lte_samsung-slsi.txt', 'samsung-slsi'),
        ProprietaryFile('vendor-tools/proprietary-files_P_gracerlte_samsung-slsi.txt', 'samsung-slsi'),
        ProprietaryFile('vendor-tools/proprietary-files_R_oss_hwc.txt', 'samsung-slsi'),
    ],
)

if __name__ == '__main__':
    utils = ExtractUtils.device(module)
    utils.run()
