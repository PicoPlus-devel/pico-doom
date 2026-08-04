# Vendored copies of Frank's pico_shared driver library.
# Upstream: https://github.com/... /pico-infonesPlus/pico_shared/drivers
#
# Each subdirectory declares an INTERFACE library:
#   pico_hdmi                (HSTX video + HDMI data-island audio)
#   pico_audio_i2s_shared    (I2S PIO+DMA, renamed to avoid clash with pico-extras)
#   tlv320dac3100            (Fruit Jam DAC + headphone-detect)
#   pico_shared_usb_hid      (USB HID host: keyboard/mouse/gamepads + xinput)
#   pico_shared_nespad       (legacy NES/SNES pads on PIO, two ports)
#   pico_shared_wiipad       (Wii-extension pads over I2C: NES/SNES Classic Mini,
#                             Wii Classic Controller (Pro))
#   i2c_bus_recovery         (standard SCL-pulse bus clear; used by the DAC and
#                             the Wii pad, which share SDA/SCL)
#   pico_fatfs               (FatFs R0.15 + SD card over HW-SPI / PIO-SPI)
#   pico_psram               (RP2350 QMI CS1 PSRAM init; trimmed, see its CMakeLists)
#
# Resync by re-copying from pico_shared/drivers, then re-apply the local patches:
#   pico_audio_i2s/CMakeLists.txt  target renamed (clashes with pico-extras)
#   nespad/nespad.{h,cpp}          nespad_read_ready(), the non-blocking "finish
#                                  will not block" check i_usbhid.cpp gates on
#   pico_hdmi/hstx.h               dropped a second declaration of
#                                  video_output_core1_run() that video_output.h
#                                  (included there) already provides
#   pico_psram/SetupPsram.c        cast a uint32_t for %x in a printf
# The last three are warning/robustness fixes worth pushing upstream.
# usb_hid is copied from the pico_shared root
# (hid_app.cpp, gamepad.cpp, gamepad.h) and depends on the 3rdparty/tusb_xinput
# submodule for the XInput host class driver. nespad and wiipad are copied from
# the pico_shared root as well (nespad.h, nespad.cpp, nespad.pio; wiipad.h,
# wiipad.cpp). pico_fatfs omits upstream's tests/, doc/ and
# fatfs_R0.15Patch1/ (not built there either); pico_psram compiles
# SetupPsram.c only.

# i2c_bus_recovery sits outside the PICO_ON_DEVICE guard because
# tlv320dac3100 links it, and that library is declared unconditionally.
add_subdirectory(drivers/i2c_bus_recovery)
add_subdirectory(drivers/tlv320dac3100)
add_subdirectory(drivers/pico_audio_i2s)
add_subdirectory(drivers/pico_hdmi)
if (PICO_ON_DEVICE)
    add_subdirectory(usb_hid)
    add_subdirectory(nespad)
    # Device-only on purpose: wiipad.cpp guards itself with
    # `#if WII_PIN_SDA >= 0`, which an undefined macro would satisfy. Only
    # device builds force-include a board header that defines the pins.
    add_subdirectory(wiipad)
    # SD card: save games and settings on every board, plus the WHD itself on
    # the doom_tiny_full WHD_LOAD_FROM_SD variant, which also needs the PSRAM.
    add_subdirectory(drivers/pico_fatfs)
    add_subdirectory(drivers/pico_psram)
endif()
