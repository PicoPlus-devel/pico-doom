#!/bin/sh
# HW_CONFIG 13: Murmulator M2 (RP2350 Pico 2 module) — HSTX video, PCM5100A
# DAC, two NES/SNES ports. Native USB host: ENABLE_PIO_USB=0 must match the
# absence of HAS_USBPIO in murmulatorm2_cflags.h (a mismatch fails the build).
# DOOM_NO_STDIO_UART keeps stdio off uart0 — GPIO 0/1 are the M2 Wii connector.
cd "$(dirname "$0")" || exit 1
TAG=murmulatorm2
. ./pico-env.sh
BUILD=build_${TAG}
export CFLAGS="-include $(pwd)/${TAG}_cflags.h -g3 -ggdb"
export CXXFLAGS="-include $(pwd)/${TAG}_cflags.h -g3 -ggdb"
cmake -S . -B $BUILD \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DPICO_SDK_PATH="$PICO_SDK_PATH" \
    -DPICO_EXTRAS_PATH="$PICO_EXTRAS_PATH" \
    -DPICOTOOL_FETCH_FROM_GIT_PATH="$(pwd)/picotool" \
    -DPICO_BOARD=pico2 \
    -DUSE_HSTX=1 \
    -DENABLE_PIO_USB=0 \
    -DDOOM_NO_STDIO_UART=1 \
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
# murmulatorm2_cflags.h. Fits a genuine 4 MB Pico 2. (The bootloader build
# uses a different map -- see murmulatorm2-build-forbootloader.sh.)
"$PICOTOOL" uf2 convert doom1.whx -t bin $BUILD/src/doom1-whx.uf2 -o 0x10080000 --family data
