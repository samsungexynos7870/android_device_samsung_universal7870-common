# Low memory optimization
# PRODUCT_PROPERTY_OVERRIDES += \
#    ro.config.low_ram=true

# Audio
PRODUCT_PROPERTY_OVERRIDES += \
    af.fast_track_multiplier=1 \
    audio_hal.force_voice_config=wide

#(OSS audio hal only) use it when faceing call
# echo with your device to avoid quit mics on other devices
# TODO: move that thing to device speciffic props
#PRODUCT_PROPERTY_OVERRIDES += \
#    audio_hal.disable_two_mic=true

# Bluetooth
PRODUCT_PROPERTY_OVERRIDES += \
    ro.bt.bdaddr_path=/efs/bluetooth/bt_addr

# Charger
PRODUCT_PRODUCT_PROPERTIES += \
    ro.charger.enable_suspend=true

# Dalvik/Art
PRODUCT_PROPERTY_OVERRIDES += \
    dalvik.vm.dex2oat-filter=speed \
    dalvik.vm.image-dex2oat-filter=speed \
    ro.sys.fw.dex2oat_thread_count=8 \
    dalvik.vm.boot-dex2oat-threads=8 \
    dalvik.vm.dex2oat-threads=8 \
    dalvik.vm.heapstartsize=8m \
    dalvik.vm.heapgrowthlimit=192m \
    dalvik.vm.heapsize=512m \
    dalvik.vm.heaptargetutilization=0.75 \
    dalvik.vm.heapminfree=512k \
    dalvik.vm.heapmaxfree=8m

# Do not update the recovery image
PRODUCT_PROPERTY_OVERRIDES += \
    persist.vendor.recovery_update=false

# FIFO UI scheduling
PRODUCT_PROPERTY_OVERRIDES += \
    sys.use_fifo_ui=1

# Graphics
PRODUCT_PROPERTY_OVERRIDES += \
    ro.opengles.version=196610 \
    debug.hwc.skip_dma_types=0,2 \
    debug.renderengine.backend=gles
    
# HWUI
PRODUCT_PROPERTY_OVERRIDES += \
    debug.hwui.skia_atrace_enabled=false \
    debug.hwui.use_buffer_age=false

# SurfaceFlinger
PRODUCT_PROPERTY_OVERRIDES += \
    ro.surface_flinger.max_frame_buffer_acquired_buffers=3

# Lockscreen rotation
PRODUCT_PROPERTY_OVERRIDES += \
    lockscreen.rot_override=true

# Nfc
PRODUCT_PROPERTY_OVERRIDES += \
    ro.nfc.port=I2C \
    ro.nfc.sec_hal=true

# Radio
PRODUCT_PROPERTY_OVERRIDES += \
    persist.radio.sib16_support=0 \
    ro.ril.hsxpa=1 \
    ro.ril.gprsclass=10 \
    ro.ril.telephony.mqanelements=6 \
    ro.telephony.default_network=9 \
    ro.telephony.get_imsi_from_sim=true \
    ro.ril.force_eri_from_xml=true \
    persist.radio.add_power_save=1 \
    persist.radio.apm_sim_not_pwdn=1 \
    ro.smps.enable=true \
    telephony.lteOnCdmaDevice=0 \

#Treble
PRODUCT_PROPERTY_OVERRIDES += \
    persist.media.treble_omx=true

# sdcardfs
PRODUCT_PROPERTY_OVERRIDES += \
    ro.sys.sdcardfs=true

# LiveDisplay
PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.sf.color_mode=0

# OMX
PRODUCT_PROPERTY_OVERRIDES += \
    debug.stagefright.ccodec=0 \
    media.stagefright.legacyencoder=true
    ro.vendor.cscsupported=1

# mediacodec cameraserver race workaround
PRODUCT_PROPERTY_OVERRIDES += \
    debug.fdsan=warn_once

# Bpf
PRODUCT_PROPERTY_OVERRIDES += \
	ro.kernel.ebpf.supported=false

# Wifi
PRODUCT_PROPERTY_OVERRIDES += \
    wifi.interface=wlan0 \
    wifi.direct.interface=p2p-dev-wlan0 \
    net.tethering.noprovisioning=true

# Blur
PRODUCT_PROPERTY_OVERRIDES += \
   ro.surface_flinger.supports_background_blur=0 \
   persist.sys.sf.disable_blurs=1 \
   ro.sf.blurs_are_expensive=1
