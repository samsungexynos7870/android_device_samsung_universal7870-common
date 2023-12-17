#
# Copyright (C) 2022 The LineageOS Project
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

LOCAL_PATH := device/samsung/universal7870-common

$(warning ********************************************************************)
$(warning * Using exynos7870 prebuilt audio setup instead of opensource one. *)
$(warning ********************************************************************)

# Audio configuration for prebuilt samsung stock audio
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/audio/prebuilt/audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_configuration.xml \
    $(LOCAL_PATH)/configs/audio/prebuilt/audio_effects.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_effects.xml

# call the proprietary audio setup
$(call inherit-product, vendor/samsung/universal7870-common/universal7870-common-prebuilt-audio_vendor.mk)
