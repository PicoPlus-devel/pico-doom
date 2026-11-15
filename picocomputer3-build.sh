#!/bin/sh
# HW_CONFIG 17: the Pico Computer 3 (RP2350B, 16 MB flash, 8 MB PSRAM) — HSTX
# video, PCM5102 DAC, two NES/SNES ports and a Wii port on the I/O header, SD
# on SPI1. Native USB host behind the board's hub: ENABLE_PIO_USB=0 must match
# the absence of HAS_USBPIO in picocomputer3_cflags.h (a mismatch fails the
# build). PICO_BOARD=weact_studio_rp2350b_core ships with the pico-sdk; the SD
# pins need its RP2350B pin tables. The serial console is UART1 on GPIO 8/9.
cd "$(dirname "$0")" || exit 1
TAG=picocomputer3
. ./pico-env.sh
BUILD=build_${TAG}
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
# Standalone (non-bootloader) build: doom owns flash from 0x10000000 and the
# WHX is flashed at 0x10080000, matching the non-bootloader TINY_WAD_ADDR in
# picocomputer3_cflags.h. (The bootloader build uses a different map -- see
# picocomputer3-build-forbootloader.sh.)
"$PICOTOOL" uf2 convert doom1.whx -t bin $BUILD/src/doom1-whx.uf2 -o 0x10080000 --family data
