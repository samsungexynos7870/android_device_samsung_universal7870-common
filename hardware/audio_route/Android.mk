LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libaudior7870
LOCAL_MODULE_TAGS := optional
LOCAL_MULTILIB := 32
LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES := audio_route.c

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    libutils \
    libexpat \
    libalsa7870

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/../tinyalsa/include

LOCAL_CFLAGS := \
    -Werror \
    -Wall

include $(BUILD_SHARED_LIBRARY)