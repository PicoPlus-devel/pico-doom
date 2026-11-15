#!/bin/sh
# HW_CONFIG 17 (the Pico Computer 3), doom_tiny_full variant: WHD_SUPER_TINY=0
# and no WAD in flash — at boot the game copies /roms/doom/doom.whd from the SD
# card into the board's PSRAM (QMI CS1 = GPIO 47, see src/pico/whd_sdload.c).
# Native USB host: ENABLE_PIO_USB=0. Generate the WHD from a
# registered/Ultimate doom.wad with:
#   whd_gen doom.wad doom.whd -no-super-tiny
# Artifact: build_full_picocomputer3/src/doom_tiny_full.uf2
cd "$(dirname "$0")" || exit 1
TAG=picocomputer3
. ./pico-env.sh
BUILD=build_full_${TAG}
export CFLAGS="-include $(pwd)/${TAG}_cflags.h -g3 -ggdb"
export CXXFLAGS="-include $(pwd)/${TAG}_cflags.h -g3 -ggdb"
cmake -S . -B $BUILD \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DPICO_SDK_PATH="$PICO_SDK_PATH" \
    -DPICO_EXTRAS_PATH="$PICO_EXTRAS_PATH" \
    -DPICOTOOL_FETCH_FROM_GIT_PATH="$(pwd)/picotool" \
    -DPICO_BOARD=weact_studio_rp2350b_core \
    -DUSE_HSTX=1 \
    -DENABLE_PIO_USB=0 \
    -DWHD_LOAD_FROM_SD=ON \
    ${CMAKE_ARGS} "$@" || exit 1

# No doom1-whx.uf2 step: the WAD data comes from the SD card at boot.
make -C $BUILD -j$(nproc) || exit 1
