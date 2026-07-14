#!/bin/sh
# HW_CONFIG 13 (Murmulator M2), doom_tiny_full built for the pico-bootLoader:
# image relinked into the 1.5 MB app slot at 0x10080000 (DOOM_APP_SIZE=
# 0x180000, 4 MB flash map). No WAD in flash — at boot the game copies
# /roms/doom/doom.whd from the SD card into the M2's onboard PSRAM (QMI CS1 =
# GPIO 8, see src/pico/whd_sdload.c). Generate the WHD from a registered/
# Ultimate doom.wad with:
#   whd_gen doom.wad doom.whd -no-super-tiny
# Artifact: build_bl_full_murmulatorm2/src/doom_tiny_full.uf2
TAG=murmulatorm2
BUILD=build_bl_full_${TAG}
export CFLAGS="-include $(pwd)/${TAG}_cflags.h -g3 -ggdb"
export CXXFLAGS="-include $(pwd)/${TAG}_cflags.h -g3 -ggdb"
cmake -S . -B $BUILD \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DPICO_SDK_PATH=3rdparty/pico-sdk \
    -DPICO_EXTRAS_PATH=3rdparty/pico-extras \
    -DPICOTOOL_FETCH_FROM_GIT_PATH="$(pwd)/picotool" \
    -DPICO_BOARD=pico2 \
    -DUSE_HSTX=1 \
    -DENABLE_PIO_USB=0 \
    -DDOOM_NO_STDIO_UART=1 \
    -DBUILD_FOR_BOOTLOADER=ON \
    -DDOOM_APP_SIZE=0x180000 \
    -DDOOM_FLASH_TOTAL=0x400000 \
    -DWHD_LOAD_FROM_SD=ON \
    ${CMAKE_ARGS} "$@"

# No doom1-whx.uf2 step: the WAD data comes from the SD card at boot.
make -C $BUILD -j$(nproc)
