#!/bin/sh
# HW_CONFIG 13 (Murmulator M2) built for the pico-bootLoader.
# NOTE: the bootloader flash map puts the WHX at 0x10400000 (the 4 MB mark) --
# this variant needs a >= 8 MB flash RP2350 module (e.g. 16 MB Pico 2 clones);
# a genuine 4 MB Pico 2 can only run the standalone murmulatorm2-build.sh.
TAG=murmulatorm2
BUILD=build_bl_${TAG}
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
    ${CMAKE_ARGS} "$@"

make -C $BUILD -j$(nproc)

PICOTOOL=./picotool/picotool-build/picotool
if [ ! -x "$PICOTOOL" ]; then
    PICOTOOL=$(command -v picotool)
fi
if [ -z "$PICOTOOL" ]; then
    echo "picotool not found (looked at ./picotool/picotool-build/picotool and PATH)" >&2
    exit 1
fi
# pico-bootLoader flash map: WHX lives at 0x10400000 (past the 3 MB slot
# reserved for the doom_tiny image at 0x10080000). Must match TINY_WAD_ADDR
# in murmulatorm2_cflags.h (the BUILD_FOR_BOOTLOADER branch).
"$PICOTOOL" uf2 convert doom1.whx -t bin $BUILD/src/doom1-whx-for-${TAG}.uf2 -o 0x10400000 --family data
