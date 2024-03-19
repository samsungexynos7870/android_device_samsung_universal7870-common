LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MULTILIB := 32
LOCAL_MODULE := android.hardware.keymaster@3.0-service.exynos7870
LOCAL_INIT_RC := android.hardware.keymaster@3.0-service.exynos7870.rc
LOCAL_SRC_FILES := \
    service.cpp

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    libdl \
    libbase \
    libutils \
    libhardware \
    libhidlbase \
    android.hardware.keymaster@3.0

include $(BUILD_EXECUTABLE)
