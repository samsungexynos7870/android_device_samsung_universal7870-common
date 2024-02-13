LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libgui2vendor
LOCAL_MODULE_OWNER := samsung
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES_32 := libgui2vendor.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MULTILIB := 32
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_ALLOW_UNDEFINED_SYMBOLS := true
# _ZN7android2ui4Size7INVALIDE
LOCAL_SHARED_LIBRARIES := android.hardware.graphics.bufferqueue@1.0 android.hardware.graphics.bufferqueue@2.0 android.hardware.graphics.common@1.1 android.hardware.graphics.common@1.2 \
                          android.hidl.token@1.0-utils libbase libcutils libEGL libGLESv2 libhidlbase liblog \
			  libnativewindow libsync libui libutils libvndksupport libbinder libc++ libc libm libdl libgui2vendor_shim
include $(BUILD_PREBUILT)
