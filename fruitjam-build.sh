#!/bin/sh
TAG=fruitjam
BUILD=build_${TAG}
export CFLAGS="-include $(pwd)/fruitjam_cflags.h"
export CXXFLAGS="-include $(pwd)/fruitjam_cflags.h"
cmake -S . -B $BUILD \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DPICO_SDK_PATH=../pico-sdk \
    -DPICOTOOL_FETCH_FROM_GIT_PATH="$(pwd)/picotool" \
    -DBOARD=adafruit_fruit_jam -DPICO_BOARD=adafruit_fruit_jam \
    -DUSE_HSTX=1 \
    ${CMAKE_ARGS} "$@"

make -C $BUILD -j$(nproc)
./picotool/picotool-build/picotool uf2 convert doom1.whx -t bin $BUILD/src/doom1-whx-for-fruitjam.uf2 -o 0x10080000 --family data
