#!/bin/sh
# Generate release-notes.md, the body of a GitHub release.
#
#   ./release-notes.sh v1.0 && cat release-notes.md
#
# Four parts, three of them generated:
#   1. a "what do I flash?" table, built from the board list in boards.sh so it
#      cannot drift away from what buildAll.sh actually produced;
#   2. fixed sections explaining the SD->PSRAM builds, pico-bootLoader use and
#      the whd_gen host binaries;
#   3. the CHANGELOG.md section for this tag (the only hand-written part);
#   4. a provenance footer.
cd "$(dirname "$0")" || exit 1

. ./boards.sh

TAG=${1:-}
if [ -z "$TAG" ]; then
    echo "usage: $0 <tag>" >&2
    exit 1
fi

OUT=release-notes.md
: > "$OUT"

# --- 1. what to flash -------------------------------------------------------
{
    echo "## What to flash"
    echo
    echo "Hold BOOTSEL, plug in USB, then drag **both** the firmware and the WAD"
    echo "data file onto the RPI-RP2 drive."
    echo
    echo "| Board | Firmware | WAD data |"
    echo "|---|---|---|"
} >> "$OUT"
for TAG_B in $RELEASE_BOARDS; do
    echo "| $(board_name "$TAG_B") | \`doom_tiny_${TAG_B}.uf2\` | \`doom1-whx.uf2\` |" >> "$OUT"
done

# --- 2. fixed sections ------------------------------------------------------
{
    echo
    echo "\`doom1-whx.uf2\` is the shareware DOOM1 WAD, already converted. It is the"
    echo "same file for every board (flashed at ${WHX_ADDR_STANDALONE})."
    echo
    echo "### Play a registered WAD from SD card (\`_full\` builds)"
    echo
    echo "\`doom_tiny_<board>_full.uf2\` leaves the WAD out of flash and instead copies"
    echo "\`/roms/doom/doom.whd\` from the SD card into PSRAM at boot. You supply the WAD:"
    echo
    echo '```'
    echo "whd_gen doom.wad doom.whd -no-super-tiny"
    echo '```'
    echo
    echo "Requires PSRAM on the board (onboard on the Fruit Jam and Murmulator M2; a"
    echo "Pimoroni Pico Plus 2 or a wired APS6404 on the other two). Do **not** flash a"
    echo "\`doom1-whx\` file alongside these builds."
    echo
    echo "> **Note:** the \`_full\` variants have not been verified on hardware yet."
    echo
    echo "### Using the pico-bootLoader"
    echo
    echo "Everything you need comes from the"
    echo "[pico-bootLoader](https://github.com/${GITHUB_REPOSITORY_OWNER:-fhoedemakers}/pico-bootLoader)"
    echo "release, not from here: the loader firmware, the relinked \`doom_tiny\` app and"
    echo "its matching WAD data. The files in **this** release are for flashing a board"
    echo "directly over USB."
    echo
    echo "### Converting your own WAD (\`whd_gen\`)"
    echo
    echo "Prebuilt, statically linked, no runtime to install:"
    echo
    echo "| File | Platform |"
    echo "|---|---|"
    echo "| \`whd_gen-win64.exe\` | 64-bit Windows |"
    echo "| \`whd_gen-linux-x64\` | x86_64 Linux |"
    echo "| \`whd_gen-linux-arm64\` | arm64 Linux (Raspberry Pi OS 64-bit) |"
    echo
    echo "On Linux, \`chmod +x whd_gen-linux-*\` first. Two conversions matter:"
    echo
    echo '```'
    echo "whd_gen DOOM1.WAD doom1.whx                  # WAD in flash, pairs with doom_tiny_<board>.uf2"
    echo "whd_gen doom.wad  doom.whd -no-super-tiny    # WAD on SD, pairs with doom_tiny_<board>_full.uf2"
    echo '```'
    echo
    echo "Output is byte-identical across platforms. For 32-bit Windows or macOS, build"
    echo "from source with \`src/whd_gen/build_native.sh\` (needs only a C/C++ compiler)."
    echo
} >> "$OUT"

# --- 3. changelog for this tag ----------------------------------------------
if [ -f CHANGELOG.md ]; then
    section=$(awk -v tag="$TAG" '
        # Find the heading whose text is exactly the tag, and print until the
        # next heading of the same or higher level -- deeper (###) subsections
        # belong to the release and must be kept. Exact match, so releasing v1.0
        # cannot accidentally pick up a v1.0.1 section.
        /^#+[ \t]/ {
            level = match($0, /[^#]/) - 1
            text = $0
            sub(/^#+[ \t]+/, "", text)
            sub(/[ \t]+$/, "", text)
            if (found) {
                if (level <= found_level) { exit }
            } else if (text == tag) {
                found = 1; found_level = level; print; next
            }
        }
        found { print }
    ' CHANGELOG.md)
    if [ -n "$section" ]; then
        printf '%s\n\n' "$section" >> "$OUT"
    else
        echo "NOTE: CHANGELOG.md has no section for '$TAG'; appending the whole file." >&2
        echo "## Changes" >> "$OUT"
        echo >> "$OUT"
        cat CHANGELOG.md >> "$OUT"
        echo >> "$OUT"
    fi
else
    echo "NOTE: no CHANGELOG.md; release notes carry no change list." >&2
fi

# --- 4. provenance ----------------------------------------------------------
{
    echo "---"
    echo
    printf 'Built from `%s`' "${GITHUB_SHA:-$(git rev-parse --short HEAD 2>/dev/null || echo unknown)}"
    if [ -n "$PICO_SDK_PATH" ] && [ -d "$PICO_SDK_PATH" ]; then
        sdk=$(git -C "$PICO_SDK_PATH" describe --tags 2>/dev/null)
        [ -n "$sdk" ] && printf ' against Pico SDK `%s`' "$sdk"
    fi
    printf ', version stamp `%s`.\n' "$TAG"
} >> "$OUT"

echo "Wrote $OUT"
