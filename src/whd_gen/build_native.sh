#!/usr/bin/env bash
#
# Standalone native build of the whd_gen WAD->WHD/WHX conversion tool.
#
# whd_gen is a host-only tool and needs no SDL / pico-sdk. The top-level
# CMake project can also build it (cmake .. && make whd_gen), but that
# requires SDL2/SDL2_mixer/SDL2_net dev packages for the rest of the tree
# even though whd_gen itself does not. This script builds just whd_gen with
# nothing but a C/C++ toolchain.
#
# Usage:
#   ./build_native.sh            # builds ./whd_gen for this machine
#   ./build_native.sh linux-x64  # static x86_64 Linux -> ./whd_gen-linux-x64
#   ./build_native.sh linux-arm64 # static aarch64 Linux -> ./whd_gen-linux-arm64
#   ./build_native.sh win64      # MinGW cross-build -> ./whd_gen-win64.exe
#   ./build_native.sh win32      # MinGW cross-build -> ./whd_gen-win32.exe
#   CXX=clang++ CC=clang ./build_native.sh
#
# The Windows targets need the MinGW-w64 cross toolchain, e.g. on Debian/
# Ubuntu: sudo apt install g++-mingw-w64-x86-64 (win64) or
# g++-mingw-w64-i686 (win32). The resulting .exe is statically linked and
# runs standalone. The script also works as-is in an MSYS2/MinGW shell on
# Windows (plain ./build_native.sh there).
#
# native vs linux-x64: `native` links dynamically and keeps debug info, which is
# what you want while developing. The linux-* targets link -static and strip, so
# the binary runs on distributions older than the build host -- these are the
# ones published in a release (see update-prebuilt-whd_gen.sh).
#
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"   # src/whd_gen
src="$(cd "$here/.." && pwd)"                           # src

target="${1:-native}"
case "$target" in
    native)
        cc_default=cc
        cxx_default=c++
        out="$here/whd_gen"
        ldflags=()
        ;;
    linux-x64)
        cc_default=gcc
        cxx_default=g++
        out="$here/whd_gen-linux-x64"
        ldflags=(-static -s)
        ;;
    linux-arm64)
        # Prefix differs per toolchain: the Debian cross package uses
        # aarch64-linux-gnu-, the ARM GNU Toolchain tarball uses
        # aarch64-none-linux-gnu-. Override with CC=/CXX= for the latter.
        cc_default=aarch64-linux-gnu-gcc
        cxx_default=aarch64-linux-gnu-g++
        out="$here/whd_gen-linux-arm64"
        ldflags=(-static -s)
        ;;
    win64)
        cc_default=x86_64-w64-mingw32-gcc
        cxx_default=x86_64-w64-mingw32-g++
        out="$here/whd_gen-win64.exe"
        ldflags=(-static -s)
        ;;
    win32)
        cc_default=i686-w64-mingw32-gcc
        cxx_default=i686-w64-mingw32-g++
        out="$here/whd_gen-win32.exe"
        ldflags=(-static -s)
        ;;
    *)
        echo "usage: $0 [native|linux-x64|linux-arm64|win64|win32]" >&2
        exit 1
        ;;
esac

CC="${CC:-$cc_default}"
CXX="${CXX:-$cxx_default}"
CFLAGS="${CFLAGS:--O2}"
CXXFLAGS="${CXXFLAGS:--O2}"

if ! command -v "$CXX" >/dev/null 2>&1; then
    echo "error: $CXX not found" >&2
    case "$target" in
        win64) echo "hint: sudo apt install g++-mingw-w64-x86-64" >&2 ;;
        win32) echo "hint: sudo apt install g++-mingw-w64-i686" >&2 ;;
        linux-arm64)
            # Deliberately NOT "apt install g++-aarch64-linux-gnu": on Ubuntu
            # 24.04 that package Breaks/Replaces gcc-multilib and g++-multilib,
            # so apt removes them -- which breaks any -m32 build on the machine.
            echo "hint: do NOT 'apt install g++-aarch64-linux-gnu' -- on Ubuntu 24.04 it" >&2
            echo "      removes gcc-multilib/g++-multilib, breaking -m32 builds." >&2
            echo "      Use a standalone toolchain tarball instead, e.g. the ARM GNU" >&2
            echo "      Toolchain (aarch64-none-linux-gnu), and point this script at it:" >&2
            echo "        CC=/opt/arm-gnu/bin/aarch64-none-linux-gnu-gcc \\" >&2
            echo "        CXX=/opt/arm-gnu/bin/aarch64-none-linux-gnu-g++ \\" >&2
            echo "          $0 linux-arm64" >&2
            ;;
    esac
    exit 1
fi

inc=(-I"$src" -I"$src/doom" -I"$here" -I"$src/adpcm-xq")
def=(-DIS_WHD_GEN=1)
if [[ "$target" != native ]]; then
    # C99-conformant printf on msvcrt-based MinGW toolchains.
    def+=(-D__USE_MINGW_ANSI_STDIO=1)
fi

obj_dir="$(mktemp -d)"
trap 'rm -rf "$obj_dir"' EXIT

objs=()

# C sources (must be compiled as C, not C++).
for c in \
    "$src/tiny_huff.c" \
    "$src/musx_decoder.c" \
    "$src/image_decoder.c" \
    "$src/adpcm-xq/adpcm-lib.c"; do
    o="$obj_dir/$(basename "${c%.c}").o"
    echo "CC  $c"
    "$CC" -std=c11 $CFLAGS "${def[@]}" "${inc[@]}" -c "$c" -o "$o"
    objs+=("$o")
done

# C++ sources.
for cc in \
    "$here/whd_gen.cpp" \
    "$here/mus2seq.cpp" \
    "$here/huff.cpp" \
    "$here/lodepng.cpp" \
    "$here/compress_mus.cpp" \
    "$here/wad.cpp"; do
    o="$obj_dir/$(basename "${cc%.cpp}").o"
    echo "CXX $cc"
    "$CXX" -std=c++17 $CXXFLAGS "${def[@]}" "${inc[@]}" -c "$cc" -o "$o"
    objs+=("$o")
done

echo "LD  $out"
"$CXX" $CXXFLAGS "${objs[@]}" ${ldflags[@]+"${ldflags[@]}"} -o "$out"

echo "Built $out"
