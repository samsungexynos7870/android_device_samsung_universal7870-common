# Low memory optimization
# PRODUCT_PROPERTY_OVERRIDES += \
#    ro.config.low_ram=true

# Audio
PRODUCT_PROPERTY_OVERRIDES += \
    af.fast_track_multiplier=1 \
    audio_hal.force_voice_config=wide

# Bluetooth
PRODUCT_PROPERTY_OVERRIDES += \
    ro.bt.bdaddr_path=/efs/bluetooth/bt_addr

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

# Graphics
PRODUCT_PROPERTY_OVERRIDES += \
    ro.opengles.version=196610 \
    debug.hwc.skip_dma_types=0,2 \
    debug.sf.latch_unsignaled=1

# Location 
PRODUCT_PROPERTY_OVERRIDES += \
    ro.location.osnlp.package=com.google.android.gms \
    ro.location.osnlp.region.package=""

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
    ro.vendor.multisim.simslotcount=2

#Treble
PRODUCT_PROPERTY_OVERRIDES += \
    persist.media.treble_omx=true

# sdcardfs
PRODUCT_PROPERTY_OVERRIDES += \
    ro.sys.sdcardfs=true

# Wifi
PRODUCT_PROPERTY_OVERRIDES += \
    wifi.interface=wlan0 \
    net.tethering.noprovisioning=true \
    wifi.direct.interface=p2p-dev-wlan0 \
    wlan.wfd.hdcp=disable

# HAL1 apps - stretch bug
PRODUCT_PROPERTY_OVERRIDES += \
    vendor.camera.hal1.packagelist=com.whatsapp,com.instagram.android,com.snapchat.android,us.zoom.videomeetings
