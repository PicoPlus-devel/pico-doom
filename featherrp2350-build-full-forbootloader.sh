#!/bin/sh
# HW_CONFIG 14 (Adafruit Feather RP2350 + TLV320), doom_tiny_full built for
# the pico-bootLoader: image relinked into the 3 MB app slot at 0x10080000.
# No WAD in flash — at boot the game copies /roms/doom/doom.whd from the SD
# card into PSRAM (see src/pico/whd_sdload.c). NOTE: this variant needs an
# APS6404 PSRAM wired to QMI CS1 = GPIO 8 (see featherrp2350_cflags.h).
# Generate the WHD from a registered/Ultimate doom.wad with:
#   whd_gen doom.wad doom.whd -no-super-tiny
# Artifact: build_bl_full_featherrp2350/src/doom_tiny_full.uf2
cd "$(dirname "$0")" || exit 1
TAG=featherrp2350
# USB host runs on PIO: PICO_PIO_USB_PATH is required (matches HAS_USBPIO).
NEED_PIO_USB=1
. ./pico-env.sh
BUILD=build_bl_full_${TAG}
export CFLAGS="-include $(pwd)/${TAG}_cflags.h -g3 -ggdb"
export CXXFLAGS="-include $(pwd)/${TAG}_cflags.h -g3 -ggdb"
cmake -S . -B $BUILD \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DPICO_SDK_PATH="$PICO_SDK_PATH" \
    -DPICO_EXTRAS_PATH="$PICO_EXTRAS_PATH" \
    -DPICOTOOL_FETCH_FROM_GIT_PATH="$(pwd)/picotool" \
    -DPICO_BOARD=adafruit_feather_rp2350 \
    -DUSE_HSTX=1 \
    -DBUILD_FOR_BOOTLOADER=ON \
    -DWHD_LOAD_FROM_SD=ON \
    ${CMAKE_ARGS} "$@" || exit 1

# No doom1-whx.uf2 step: the WAD data comes from the SD card at boot.
make -C $BUILD -j$(nproc) || exit 1
