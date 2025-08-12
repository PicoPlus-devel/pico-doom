#!/bin/sh
TAG=fruitjam
export CFLAGS="-include $(pwd)/fruitjam_cflags.h"
export CXXFLAGS="-include $(pwd)/fruitjam_cflags.h"
cmake -S . -B build_${TAG} \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DPICO_SDK_PATH=../pico-sdk \
    -DPICOTOOL_FETCH_FROM_GIT_PATH="$(pwd)/picotool" \
    -DBOARD=adafruit_fruit_jam -DPICO_BOARD=adafruit_fruit_jam \
    -DUSE_HSTX=1 \
    ${CMAKE_ARGS} "$@"

make -C build_${TAG} -j$(nproc)
