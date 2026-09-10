# Copyright (C) 2017 The LineageOS Project
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

ifeq ($(BOARD_USE_SPKAMP),true)

LOCAL_PATH := $(call my-dir)

# The amplifier IC differs per device: TFA9890 or TFA9896. Each model gets
# its own source directory and its own register map/container file name
# (see hardware/tfa98xx/tfa98xx_cust.h). Device trees select it with
# TARGET_BOARD_TFA_MODEL, same as on lineage-18.1-lts.
ifeq ($(filter $(TARGET_BOARD_TFA_MODEL),9890 9896),)
$(error TARGET_BOARD_TFA_MODEL must be set to 9890 or 9896 when BOARD_USE_SPKAMP is true)
endif

include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libutils \
	libcutils \
	libhardware \
	libtfa98xx_oss \
	libtinyalsa

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
	external/tinyalsa/include \
	external/tinycompress/include \
	hardware/libhardware/include \
	hardware/samsung/audio \
	$(call include-path-for, audio-utils) \
	$(call include-path-for, audio-route) \
	$(call include-path-for, audio-effects)

LOCAL_SRC_FILES := \
	tfa$(TARGET_BOARD_TFA_MODEL)/amplifier.c \
	tfa$(TARGET_BOARD_TFA_MODEL)/tfa.c

LOCAL_CFLAGS := -Werror -Wall
LOCAL_CFLAGS += -DPREPROCESSING_ENABLED
LOCAL_CFLAGS += -DTFA_MODEL_$(TARGET_BOARD_TFA_MODEL)

LOCAL_MODULE := audio_amplifier.$(TARGET_BOOTLOADER_BOARD_NAME)
LOCAL_VENDOR_MODULE := true
LOCAL_MULTILIB := 32
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

endif
