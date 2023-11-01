# Copyright (C) 2014 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#
# Primary Audio HAL
#
ifeq ($(TARGET_AUDIOHAL_VARIANT),samsung-linaro-exynos7870)
LOCAL_PATH := $(call my-dir)

LOCAL_ARM_MODE := arm

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	audio_hw.c \
	sec/voice_manager.c \
	audio_route.c

# inline libaudioroute for vendor support

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/include \
	external/tinyalsa/include \
	external/tinycompress/include \
	external/kernel-headers/original/uapi/sound \
	hardware/libhardware/include \
	$(call include-path-for, audio-route) \
	$(call include-path-for, audio-utils) \
	$(call include-path-for, audio-effects) \
	external/expat/lib

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libcutils \
	libhardware \
	libprocessgroup \
	libtinyalsa \
	libtinycompress \
	libaudioroute \
	libaudioutils \
	libdl \
	libexpat

LOCAL_CFLAGS := -Werror -Wall
#LOCAL_CFLAGS += -DPREPROCESSING_ENABLED
#LOCAL_CFLAGS += -DHW_AEC_LOOPBACK

LOCAL_MODULE := audio.primary.$(TARGET_BOOTLOADER_BOARD_NAME)
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_VENDOR_MODULE := true

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
endif
