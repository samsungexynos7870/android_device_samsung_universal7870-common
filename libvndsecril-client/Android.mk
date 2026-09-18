# Copyright 2006 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    secril-client.cpp

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libbinder \
    libcutils \
    libhardware_legacy \
    liblog

LOCAL_CFLAGS :=

ifneq ($(filter m7450 mdm9x35 ss333 xmm7260,$(BOARD_MODEM_TYPE)),)
LOCAL_CFLAGS += -DSAMSUNG_NEXT_GEN_MODEM
endif

ifeq ($(TARGET_USES_VND_SECRIL), true)
LOCAL_CFLAGS += -DUSES_VND_SECRIL
endif

# Drop-in replacement for the vendor prebuilt libvndsecril-client.so:
# same module name, SONAME and exported API, built from source.
LOCAL_MODULE := libvndsecril-client
LOCAL_MULTILIB := both
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
