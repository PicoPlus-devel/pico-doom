#!/bin/sh
# HW_CONFIG 8 (Adafruit Fruit Jam), doom_tiny_full variant: WHD_SUPER_TINY=0
# and no WAD in flash — at boot the game copies /roms/doom/doom.whd from the
# SD card into the 8 MB onboard PSRAM (see src/pico/whd_sdload.c). Generate
# the WHD from a registered/Ultimate doom.wad with:
#   whd_gen doom.wad doom.whd -no-super-tiny
# Artifact: build_full_fruitjam/src/doom_tiny_full.uf2
TAG=fruitjam
BUILD=build_full_${TAG}
export CFLAGS="-include $(pwd)/${TAG}_cflags.h -g3 -ggdb"
export CXXFLAGS="-include $(pwd)/${TAG}_cflags.h -g3 -ggdb"
cmake -S . -B $BUILD \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DPICO_SDK_PATH=3rdparty/pico-sdk \
    -DPICO_EXTRAS_PATH=3rdparty/pico-extras \
    -DPICOTOOL_FETCH_FROM_GIT_PATH="$(pwd)/picotool" \
    -DBOARD=adafruit_fruit_jam -DPICO_BOARD=adafruit_fruit_jam \
    -DUSE_HSTX=1 \
    -DWHD_LOAD_FROM_SD=ON \
    ${CMAKE_ARGS} "$@"

# No doom1-whx.uf2 step: the WAD data comes from the SD card at boot.
make -C $BUILD -j$(nproc)
