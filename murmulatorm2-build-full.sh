#!/bin/sh
# HW_CONFIG 13 (Murmulator M2, RP2350 Pico 2 module), doom_tiny_full variant:
# WHD_SUPER_TINY=0 and no WAD in flash — at boot the game copies
# /roms/doom/doom.whd from the SD card into the M2's onboard PSRAM (QMI CS1 =
# GPIO 8, see src/pico/whd_sdload.c). Native USB host: ENABLE_PIO_USB=0 must
# match the absence of HAS_USBPIO in murmulatorm2_cflags.h. DOOM_NO_STDIO_UART
# keeps stdio off uart0 — GPIO 0/1 are the M2 Wii connector. Generate the WHD
# from a registered/Ultimate doom.wad with:
#   whd_gen doom.wad doom.whd -no-super-tiny
# Artifact: build_full_murmulatorm2/src/doom_tiny_full.uf2
TAG=murmulatorm2
BUILD=build_full_${TAG}
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
    -DWHD_LOAD_FROM_SD=ON \
    ${CMAKE_ARGS} "$@"

# No doom1-whx.uf2 step: the WAD data comes from the SD card at boot.
make -C $BUILD -j$(nproc)
