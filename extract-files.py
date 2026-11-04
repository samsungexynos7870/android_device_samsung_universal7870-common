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

import os

# The dump driver (extract-files-exynos7870.sh) runs one proprietary-files list
# per firmware dump; EXTRACT_UTILS_ONLY_LIST selects a single list so files
# absent from that dump are not treated as fatal "file not found".
_EXTRACT_UTILS_ONLY_LIST = os.environ.get('EXTRACT_UTILS_ONLY_LIST')


def _proprietary_file(file_list_path: str, vendor_rel_sub_path: str = 'proprietary'):
    if _EXTRACT_UTILS_ONLY_LIST and _EXTRACT_UTILS_ONLY_LIST not in file_list_path:
        return None
    return ProprietaryFile(file_list_path, vendor_rel_sub_path)

HWCOMPOSER_ORIG_HEX = '73 79 73 2F 64 65 76 69 63 65 73 2F 00 31 34 38 33 30 30 30 30 2E 64 65 63 6F 6E 5F 66 2F 76 73 79 6E 63 00 31 34 38 36 30 30 30 30 2E 73 79 73 6D 6D 75 2F 31 34 38 36 30 30 30 30 2E 73 79 73 6D 6D 75 2F 00 65 78 79 6E 6F 73 35 2D 66 62 2E 31 2F 76 73 79 6E 63 00 70 6C 61 74 66 6F 72 6D 2F 65 78 79 6E 6F 73 2D 73 79 73 6D 6D 75 2E 33 30 2F 65 78 79 6E 6F 73 2D 73 79 73 6D 6D 75 2E 31 31 2F 00 66 61 69 6C 65 64 20'
HWCOMPOSER_NEW_HEX = '73 79 73 2F 64 65 76 69 63 65 73 2F 00 31 34 38 33 30 30 30 30 2E 64 65 63 6F 6E 5F 66 62 2F 76 73 79 6E 63 00 31 34 38 35 30 30 30 30 2E 73 79 73 6D 6D 75 2F 31 34 38 35 30 30 30 30 2E 73 79 73 6D 6D 75 2F 00 65 78 79 6E 6F 73 35 2D 66 62 2E 31 2F 76 73 79 6E 63 00 70 6C 61 74 66 6F 72 6D 2F 65 78 79 6E 6F 73 2D 73 79 73 6D 6D 75 2E 33 30 2F 65 78 79 6E 6F 73 2D 73 79 73 6D 6D 75 2E 31 31 2F 00 66 61 69 6C 64 20'

blob_fixups: blob_fixups_user_type = {
    'vendor/lib/hw/audio.primary.exynos7870.so': blob_fixup()
        .replace_needed('libtinyalsa.so', 'libalsa7870.so')
        .replace_needed('libaudioroute.so', 'libaudior7870.so'),
    ('vendor/lib/libpreprocessing_nxp.so', 'vendor/lib64/libpreprocessing_nxp.so'): blob_fixup()
        .replace_needed('libtinyalsa.so', 'libalsa7870.so'),
    'vendor/lib/lib_SamsungRec_06004.so': blob_fixup()
        .replace_needed('libstdc++.so', 'libc++.so'),
    ('vendor/lib/hw/memtrack.exynos7870.so', 'vendor/lib64/hw/memtrack.exynos7870.so'): blob_fixup()
        .sig_replace(
            '6D 65 6D 74 72 61 63 6B 2E 75 6E 69 76 65 72 73 61 6C 37 38 38 30 2E 73 6F',
            '6D 65 6D 74 72 61 63 6B 2E 75 6E 69 76 65 72 73 61 6C 37 38 37 30 2E 73 6F',
        ),
    ('vendor/lib/hw/hwcomposer.exynos7870.so', 'vendor/lib64/hw/hwcomposer.exynos7870.so'): blob_fixup()
        .sig_replace(HWCOMPOSER_ORIG_HEX, HWCOMPOSER_NEW_HEX),
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
        .regex_replace('(?m)^Agnss_ShaVariant=0$', 'Agnss_ShaVariant=256'),
}  # fmt: skip

module = ExtractUtilsModule(
    'universal7870-common',
    'samsung',
    blob_fixups=blob_fixups,
    skip_main_proprietary_file=True,
    proprietary_files=[
        pf for pf in (
        _proprietary_file('vendor-tools/proprietary-files_Q_a6lte_audio.txt', 'audio/sec'),
        _proprietary_file('vendor-tools/proprietary-files_Q_m10lte_audio.txt', 'audio/sec_tfa'),
        _proprietary_file('vendor-tools/proprietary-files_Q_a6lte_gnss.txt', 'gnss'),
        _proprietary_file('vendor-tools/proprietary-files_Q_a6lte_secapp.txt', 'secapp'),
        _proprietary_file('vendor-tools/proprietary-files_Q_m10lte_radio.txt', 'radio'),
        _proprietary_file('vendor-tools/proprietary-files_Q_j7duolte_radio.txt', 'radio'),
        _proprietary_file('vendor-tools/proprietary-files_Q_m10lte_sensors.txt', 'sensors'),
        _proprietary_file('vendor-tools/proprietary-files_Q_m10lte_media.txt', 'media'),
        _proprietary_file('vendor-tools/proprietary-files_Q_a6lte_drm.txt', 'drm'),
        _proprietary_file('vendor-tools/proprietary-files_P_a7y17lte_mali.txt', ''),
        _proprietary_file('vendor-tools/proprietary-files_P_a6lte_samsung-slsi.txt', 'samsung-slsi'),
        _proprietary_file('vendor-tools/proprietary-files_P_a7y17lte_samsung-slsi.txt', 'samsung-slsi'),
        _proprietary_file('vendor-tools/proprietary-files_P_gracerlte_samsung-slsi.txt', 'samsung-slsi'),
        _proprietary_file('vendor-tools/proprietary-files_R_oss_hwc.txt', 'samsung-slsi'),
        ) if pf is not None
    ],
)

if __name__ == '__main__':
    utils = ExtractUtils.device(module)
    utils.run()
