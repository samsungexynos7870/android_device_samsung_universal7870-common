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


def _proprietary_file(file_list_path: str, vendor_rel_sub_path: str = 'proprietary', **kwargs):
    if _EXTRACT_UTILS_ONLY_LIST and _EXTRACT_UTILS_ONLY_LIST not in file_list_path:
        return None
    return ProprietaryFile(file_list_path, vendor_rel_sub_path, **kwargs)

namespace_imports = [
    'device/samsung/universal7870-common',
    'hardware/google/interfaces',
    'hardware/google/pixel',
    'hardware/samsung',
    'hardware/ril',
    'hardware/lineage/compat',
    'vendor/samsung/universal7870-common',
]


blob_fixups: blob_fixups_user_type = {
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

# Custom part of the lineage-22.2 vendor Android.mk that the generator cannot
# express: the libGLES_mali prebuilt with its Vulkan/OpenCL symlinks.
_MALI_ANDROID_MK = r"""ifneq ($(filter a3y17lte j5y17lte a6lte j6lte j7velte j7xelte j7y17lte on7xelte m10lte j7popelteskt,$(TARGET_DEVICE)),)

###############################
# CUSTOM PART START EXYNOS7870#
###############################

include $(CLEAR_VARS)
LOCAL_MODULE := libGLES_mali
LOCAL_MODULE_OWNER := samsung
LOCAL_SRC_FILES_64 := proprietary/vendor/lib64/egl/libGLES_mali.so
LOCAL_SRC_FILES_32 := proprietary/vendor/lib/egl/libGLES_mali.so
LOCAL_MULTILIB := both
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_CHECK_ELF_FILES := false
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_PATH_32 := $($(TARGET_2ND_ARCH_VAR_PREFIX)TARGET_OUT_VENDOR_SHARED_LIBRARIES)/egl
LOCAL_MODULE_PATH_64 := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)/egl

SYMLINKS := $(TARGET_OUT_VENDOR)
$(SYMLINKS):
	@echo "Symlink: vulkan.$(TARGET_BOARD_PLATFORM).so"
	@mkdir -p $@/lib/hw
	@mkdir -p $@/lib64/hw
	$(hide) ln -sf ../egl/libGLES_mali.so $@/lib/hw/vulkan.$(TARGET_BOARD_PLATFORM).so
	$(hide) ln -sf ../egl/libGLES_mali.so $@/lib64/hw/vulkan.$(TARGET_BOARD_PLATFORM).so
	@echo "Symlink: libOpenCL.so"
	$(hide) ln -sf egl/libGLES_mali.so $@/lib/libOpenCL.so
	$(hide) ln -sf egl/libGLES_mali.so $@/lib64/libOpenCL.so
	@echo "Symlink: libOpenCL.so.1"
	$(hide) ln -sf egl/libGLES_mali.so $@/lib/libOpenCL.so.1
	$(hide) ln -sf egl/libGLES_mali.so $@/lib64/libOpenCL.so.1
	@echo "Symlink: libOpenCL.so.1.1"
	$(hide) ln -sf egl/libGLES_mali.so $@/lib/libOpenCL.so.1.1
	$(hide) ln -sf egl/libGLES_mali.so $@/lib64/libOpenCL.so.1.1

ALL_MODULES.$(LOCAL_MODULE).INSTALLED := \
	$(ALL_MODULES.$(LOCAL_MODULE).INSTALLED) $(SYMLINKS)

include $(BUILD_PREBUILT)

endif"""

def _post_makefiles(ctx, packages_ctx=None):
    ctx.mk_out.write(_MALI_ANDROID_MK)


module = ExtractUtilsModule(
    'universal7870-common',
    'samsung',
    blob_fixups=blob_fixups,
    namespace_imports=namespace_imports,
    skip_main_proprietary_file=True,
    proprietary_files=[
        pf for pf in (
        _proprietary_file('vendor-tools/proprietary-files_Q_a6lte_gnss.txt'),
        _proprietary_file('vendor-tools/proprietary-files_Q_a6lte_secapp.txt'),
        _proprietary_file('vendor-tools/proprietary-files_Q_m10lte_radio.txt'),
        _proprietary_file('vendor-tools/proprietary-files_Q_j7duolte_radio.txt'),
        _proprietary_file('vendor-tools/proprietary-files_Q_m10lte_sensors.txt'),
        _proprietary_file('vendor-tools/proprietary-files_Q_m10lte_media.txt'),
        _proprietary_file('vendor-tools/proprietary-files_Q_a6lte_drm.txt'),
        _proprietary_file('vendor-tools/proprietary-files_P_a7y17lte_mali.txt', post_makefile_generation_fn=_post_makefiles),
        ) if pf is not None
    ],
)

if __name__ == '__main__':
    utils = ExtractUtils.device(module)
    utils.run()
