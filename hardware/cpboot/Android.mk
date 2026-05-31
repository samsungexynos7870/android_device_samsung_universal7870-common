
LOCAL_PATH := $(call my-dir)

# -----------------------------------------------------------------------------
# Common settings shared by cbd and cld (mirrors cc_defaults)
# -----------------------------------------------------------------------------

# Base flags
cbd_common_cflags := \
    -Wall \
    -Wno-unused-parameter \
    -D_GNU_SOURCE

# Additional include paths
cbd_common_include_dirs := \
    bionic/libc/bionic

# -----------------------------------------------------------------------------
# cbd binary (cbd_v1.sipc)
# -----------------------------------------------------------------------------
# ifeq ($(CBD_V1_SIPC_ENABLED),true)

include $(CLEAR_VARS)

LOCAL_MODULE := cbd
LOCAL_PROPRIETARY_MODULE := true

LOCAL_CFLAGS := $(cbd_common_cflags)
LOCAL_C_INCLUDES := $(cbd_common_include_dirs)

LOCAL_SRC_FILES := \
    main.c \
    util.c \
    boot_xmm626x.c \
    boot_xmm72xx.c \
    boot_xmm72xx_lli.c \
    boot_cmc221.c \
    boot_cbp72.c \
    boot_esc6270.c \
    hdlc.c \
    std_boot.c \
    boot_shannon.c \
    boot_shannon_hsic.c \
    boot_shannon333.c \
    boot_shannon310.c \
    boot_shannon5100.c \
    util_srinfo.c

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils

# libc is linked implicitly
# LOCAL_SHARED_LIBRARIES += libc

include $(BUILD_EXECUTABLE)

# endif

# -----------------------------------------------------------------------------
# cld binary (cld_v1.sipc)
# -----------------------------------------------------------------------------
# ifeq ($(CBD_V1_SIPC_ENABLED),true)
# 
# include $(CLEAR_VARS)
# 
# LOCAL_MODULE := cld_v1.sipc
# LOCAL_MODULE_STEM := cld
# LOCAL_PROPRIETARY_MODULE := true
# 
# LOCAL_CFLAGS := $(cbd_common_cflags)
# LOCAL_C_INCLUDES := $(cbd_common_include_dirs)
# 
# LOCAL_SRC_FILES := lb_main.c
# 
# LOCAL_SHARED_LIBRARIES := \
#     liblog \
#     libcutils
# 
# include $(BUILD_EXECUTABLE)

# endif