LOCAL_PATH:= $(call my-dir)

# Only needed by hardware/amplifier, which is guarded by BOARD_USE_SPKAMP.
ifeq ($(BOARD_USE_SPKAMP),true)

############################## libtfa98xx
include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= 	$(LOCAL_PATH)/tfa/inc \
			$(LOCAL_PATH)/tfa/linux-user-inc \
			$(LOCAL_PATH)/tfa/src \
			$(LOCAL_PATH)/utl/inc \
			$(LOCAL_PATH)/hal/inc \
			$(LOCAL_PATH)/hal/src \
			$(LOCAL_PATH)/hal/src/lxScribo \
			$(LOCAL_PATH)/srv/inc \
			$(LOCAL_PATH)/srv/src/iniFile \
			$(LOCAL_PATH)/app/climax/src/cli

LOCAL_SRC_FILES:= 	hal/src/NXP_I2C.c  \
			hal/src/lxScribo/lxDummy.c  \
			hal/src/lxScribo/lxScribo.c \
			hal/src/lxScribo/lxScriboProtocol.c  \
			hal/src/lxScribo/lxScriboProtocolUdp.c  \
			hal/src/lxScribo/lxScriboSerial.c  \
			hal/src/lxScribo/lxScriboSocket.c\
			hal/src/lxScribo/udp_sockets.c  \
			hal/src/lxScribo/lxI2c.c \
			hal/src/lxScribo/lxSysfs.c \
			tfa/src/tfa_debug.c \
			tfa/src/tfa_hal.c \
			tfa/src/tfa_osal.c \
			tfa/src/tfa_dsp.c \
			tfa/src/tfa_container.c \
			tfa/src/tfa_container_crc32.c \
			tfa/src/tfa9888_init.c \
			tfa/src/tfa9897_init.c \
			tfa/src/tfa9891_init.c \
			tfa/src/tfa9890_init.c \
			tfa/src/tfa9887B_init.c \
			tfa/src/tfa9887_init.c \
			utl/src/tfaOsal.c \
			srv/src/tfaContainerWrite.c \
			srv/src/tfaContUtil.c \
			srv/src/iniFile/minIni.c \
			srv/src/Tfa98API.c \
			srv/src/tfa98xxDRC.c \
			srv/src/tfa98xxFilterCalculations.c \
			srv/src/tfa98xxCalculations.c \
			srv/src/tfa98xxCalibration.c \
			srv/src/tfa98xxLiveData.c \
			srv/src/tfa98xxDiagnostics.c \
			app/exTfa98xx/src/main_container.c

LOCAL_MODULE := libtfa98xx_oss
LOCAL_VENDOR_MODULE := true
LOCAL_MULTILIB := 32
LOCAL_SHARED_LIBRARIES:= libcutils libutils liblog
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_CFLAGS := -D__ANDROID__ -DTFA_DEBUG -DLXSCRIBO -DHAL_SYSFS -DI2C_FORCE -Wno-unreachable-code-loop-increment

ifeq ($(TARGET_BOARD_TFA_MODEL),9890)
    LOCAL_CFLAGS += -DTFA_MODEL_9890
endif

ifeq ($(TARGET_BOARD_TFA_MODEL),9896)
    LOCAL_CFLAGS += -DTFA_MODEL_9896
endif
include $(BUILD_SHARED_LIBRARY)

endif # BOARD_USE_SPKAMP

############################## cli app
#include $(CLEAR_VARS)

#LOCAL_C_INCLUDES:= 	${LOCAL_PATH}/app/climax/src/cli \
#			$(LOCAL_PATH)/app/climax/inc \
#			$(LOCAL_PATH)/srv/inc \
#			$(LOCAL_PATH)/tfa/inc \
#			$(LOCAL_PATH)/tfa/linux-user-inc \
#			$(LOCAL_PATH)/utl/inc \
#			$(LOCAL_PATH)/hal/inc \
#			$(LOCAL_PATH)/hal/src \
#			$(LOCAL_PATH)/hal/src/lxScribo/scribosrv \
#			$(LOCAL_PATH)/hal/src/lxScribo
#
#LOCAL_SRC_FILES:= 	app/climax/src/climax.c \
#			app/climax/src/cliCommands.c \
#			app/climax/src/cli/cmdline.c \
#			hal/src/lxScribo/scribosrv/i2cserver.c \
#			hal/src/lxScribo/scribosrv/cmd.c
#
#LOCAL_MODULE := climax
#LOCAL_SHARED_LIBRARIES:= libcutils libutils
#LOCAL_STATIC_LIBRARIES:= libtfa98xx
#LOCAL_MODULE_TAGS := eng debug
#LOCAL_PRELINK_MODULE := false
#LOCAL_CFLAGS := -D__ANDROID__
#include $(BUILD_EXECUTABLE)

