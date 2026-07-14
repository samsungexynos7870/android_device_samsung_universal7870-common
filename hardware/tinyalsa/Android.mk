ifeq ($(TARGET_BOARD_HAS_TFA_SEC_AUDIO_HAL),true)
TARGET_BOARD_HAS_SEC_ALSA := true
else ifeq ($(TARGET_BOARD_HAS_SEC_AUDIO_HAL),true)
TARGET_BOARD_HAS_SEC_ALSA := true
endif

ifeq ($(TARGET_BOARD_HAS_SEC_ALSA),true)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libalsa7870
LOCAL_MODULE_TAGS := optional
LOCAL_VENDOR_MODULE := true
LOCAL_MULTILIB := 32

# Source files
LOCAL_SRC_FILES := mixer.c pcm.c

# Local include
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
# Export headers
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include

# Compiler flags
LOCAL_CFLAGS := -Werror -Wno-macro-redefined

include $(BUILD_SHARED_LIBRARY)

endif