#!/bin/sh
# HW_CONFIG 17 (the Pico Computer 3) built for the pico-bootLoader. 16 MB
# flash, so the Fruit Jam map: bootloader (512 KB) at 0x10000000, a 1.5 MB
# emulator app slot at 0x10080000 (DOOM_APP_SIZE=0x180000) and the WHX at
# 0x10200000 (WHX_ADDR below). WHX_ADDR must match the BUILD_FOR_BOOTLOADER
# TINY_WAD_ADDR in picocomputer3_cflags.h. Goes to SD:/emu/17/ (pico-bootLoader
# does not list HW_CONFIG 17 yet). Native USB host: ENABLE_PIO_USB=0.
cd "$(dirname "$0")" || exit 1
TAG=picocomputer3
. ./pico-env.sh
BUILD=build_bl_${TAG}
WHX_ADDR=0x10200000
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
    -DBUILD_FOR_BOOTLOADER=ON \
    -DDOOM_APP_SIZE=0x180000 \
    ${CMAKE_ARGS} "$@" || exit 1

make -C $BUILD -j$(nproc) || exit 1

PICOTOOL=./picotool/picotool-build/picotool
if [ ! -x "$PICOTOOL" ]; then
    PICOTOOL=$(command -v picotool)
fi
if [ -z "$PICOTOOL" ]; then
    echo "picotool not found (looked at ./picotool/picotool-build/picotool and PATH)" >&2
    exit 1
fi
# pico-bootLoader flash map: WHX lives at 0x10200000 (past the 1.5 MB app slot
# reserved for the doom_tiny image at 0x10080000). Must match TINY_WAD_ADDR
# in picocomputer3_cflags.h (the BUILD_FOR_BOOTLOADER branch) and WHX_ADDR above.
"$PICOTOOL" uf2 convert doom1.whx -t bin $BUILD/src/doom1-whx-bootloader.uf2 -o "$WHX_ADDR" --family data
