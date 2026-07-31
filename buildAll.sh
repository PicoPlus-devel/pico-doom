#!/bin/sh
# ====================================================================================
# PICO-DOOM build all script
#
# Builds every released configuration and collects the artifacts in releases/.
# This is the whole body of the release workflow: what CI runs is exactly
# `./buildAll.sh`, so running it locally is the way to test the pipeline.
#
#   export PICO_SDK_PATH=/path/to/pico-sdk
#   export PICO_EXTRAS_PATH=/path/to/pico-extras
#   export PICO_PIO_USB_PATH=/path/to/Pico-PIO-USB
#   ./buildAll.sh
#
# Optional: CMAKE_ARGS="-DDOOM_RELEASE_VERSION=v1.0" to stamp a version into the
# UF2 binary info (visible in `picotool info`).
#
# Released per board: the standalone build (doom_tiny) and the SD->PSRAM build
# (doom_tiny_full). The pico-bootLoader variants are NOT released here -- they
# are built in the pico-bootLoader repo, which calls <tag>-build-forbootloader.sh
# directly.
# ====================================================================================
cd "$(dirname "$0")" || exit 1

. ./boards.sh

# picotool converts doom1.whx into the loadable UF2s, and the SDK needs it to
# build the UF2s themselves.
if ! command -v picotool > /dev/null 2>&1; then
    echo "picotool could not be found" >&2
    echo "Please install picotool from https://github.com/raspberrypi/picotool.git" >&2
    exit 1
fi
# BootPartition.cmake needs >= 2.2.0 for the --platform rp2350 UF2 flag.
PICOTOOL_VER=$(picotool version --semantic 2>/dev/null)
case "$PICOTOOL_VER" in
    ""|0.*|1.*|2.0*|2.1*)
        echo "picotool $PICOTOOL_VER is too old; 2.2.0 or newer is required" >&2
        exit 1 ;;
esac
echo "Using picotool $PICOTOOL_VER"

[ -f doom1.whx ] || { echo "doom1.whx missing from the repository root" >&2; exit 1; }

[ -d releases ] && rm -rf releases
mkdir releases || exit 1

# ---------------------------------------------------------------------------
# whd_gen: prebuilt and committed, never compiled here. The release runner is a
# Raspberry Pi 5 and would need a MinGW and an x86 toolchain to produce these,
# so they are cross-built on a dev box by update-prebuilt-whd_gen.sh.
# ---------------------------------------------------------------------------
PREBUILT=prebuilt/whd_gen
WHD_GEN_EXPECTED="whd_gen-linux-x64 whd_gen-linux-arm64 whd_gen-win64.exe"
missing=""
for f in $WHD_GEN_EXPECTED; do
    [ -f "$PREBUILT/$f" ] || missing="$missing $f"
done
if [ -n "$missing" ]; then
    echo "ERROR: missing prebuilt whd_gen binaries:$missing" >&2
    echo "Build them on an x86_64 Linux box and commit them:" >&2
    echo "    ./update-prebuilt-whd_gen.sh" >&2
    exit 1
fi

# Staleness check: these binaries are refreshed by hand, so they can fall behind
# the sources. Warn rather than fail -- a comment-only commit under src/whd_gen
# should not block a firmware release -- but make it loud in the CI log.
WHD_GEN_SOURCES="src/whd_gen src/tiny_huff.c src/musx_decoder.c src/image_decoder.c src/adpcm-xq"
if [ -d .git ]; then
    src_commit=$(git rev-list -1 HEAD -- $WHD_GEN_SOURCES 2>/dev/null)
    stamped=$(sed -n 's/^source-commit: *//p' "$PREBUILT/VERSIONS.txt" 2>/dev/null)
    if [ -n "$src_commit" ] && [ -n "$stamped" ] && [ "$src_commit" != "$stamped" ]; then
        echo ""
        echo "WARNING: prebuilt whd_gen binaries may be stale."
        echo "         whd_gen sources are at $src_commit"
        echo "         prebuilt/whd_gen was built from $stamped"
        echo "         Rerun ./update-prebuilt-whd_gen.sh and commit if it matters."
        echo ""
    fi
fi

# ---------------------------------------------------------------------------
# Firmware: 2 variants x 4 boards.
#
# Every board's build script writes doom_tiny.uf2 / doom_tiny_full.uf2 under the
# same basename, so the copy into releases/ has to rename per configuration or
# the boards would overwrite each other.
# ---------------------------------------------------------------------------
for TAG in $RELEASE_BOARDS; do
    echo ""
    echo "=================== $TAG (standalone) ==================="
    ./${TAG}-build.sh || exit 1
    cp "build_${TAG}/src/doom_tiny.uf2" "releases/doom_tiny_${TAG}.uf2" || exit 1

    echo ""
    echo "=================== $TAG (SD -> PSRAM) ==================="
    ./${TAG}-build-full.sh || exit 1
    cp "build_full_${TAG}/src/doom_tiny_full.uf2" "releases/doom_tiny_${TAG}_full.uf2" || exit 1
done

# ---------------------------------------------------------------------------
# WAD data. doom1.whx is board-independent and every standalone build flashes it
# at the same offset, so convert it directly rather than picking one board's
# build output.
#
# Only the standalone offset is released. The pico-bootLoader builds use two
# further offsets that depend on the board's flash size, and those WHX files are
# produced by the pico-bootLoader repo alongside the app binaries -- shipping
# them here as well would only invite flashing the wrong one.
# ---------------------------------------------------------------------------
echo ""
echo "=================== WAD data (doom1.whx) ==================="
picotool uf2 convert doom1.whx -t bin releases/doom1-whx.uf2 \
    -o "$WHX_ADDR_STANDALONE" --family data || exit 1

# ---------------------------------------------------------------------------
# whd_gen host binaries
# ---------------------------------------------------------------------------
for f in $WHD_GEN_EXPECTED; do
    cp "$PREBUILT/$f" "releases/$f" || exit 1
done

# ---------------------------------------------------------------------------
if [ -z "$(ls -A releases)" ]; then
    echo "No files found in releases folder" >&2
    exit 1
fi

echo ""
echo "=================== releases/ ==================="
for UF2 in releases/*.uf2; do
    ls -l "$UF2"
    picotool info "$UF2"
    echo " "
done
ls -l releases/whd_gen-*
echo ""
echo "$(ls releases | wc -l) files in releases/"
