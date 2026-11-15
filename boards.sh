# The board configurations a release covers.
#
# Sourced by buildAll.sh and release-notes.sh so the built artifacts and the
# "what do I flash?" table in the release notes cannot drift apart.
#
# Each board has a <tag>_cflags.h header and a <tag>-build*.sh script family.
# Only the standalone variants are released here: the pico-bootLoader variants
# (<tag>-build-forbootloader.sh) are built in the pico-bootLoader repo, which
# invokes those scripts itself.

RELEASE_BOARDS="fruitjam adafruitdvisd murmulatorm2 featherrp2350 olimexpicopc pizeroliupcb picocomputer3"

# Human-readable name, for the release notes.
board_name() {
    case "$1" in
        fruitjam)      echo "Adafruit Fruit Jam" ;;
        adafruitdvisd) echo "Pico 2 / Pico Plus 2 + Adafruit DVI breakout" ;;
        murmulatorm2)  echo "Murmulator M2 (RP2350)" ;;
        featherrp2350) echo "Adafruit Feather RP2350 + TLV320 breakout" ;;
        olimexpicopc)  echo "Olimex RP2040-PICO-PC + Pico 2" ;;
        pizeroliupcb)  echo "Waveshare RP2350-PiZero on the PCB by Liu Szuping" ;;
        picocomputer3) echo "Pico Computer 3 (PC3), not yet tested" ;;
        *)             echo "$1" ;;
    esac
}

# Where the WAD data (doom1.whx) is flashed by the standalone builds. Must match
# the non-BUILD_FOR_BOOTLOADER branch of TINY_WAD_ADDR in every <tag>_cflags.h,
# which is the same offset on all seven boards.
#
# The pico-bootLoader builds use one other offset, 0x10200000 behind their 1.5 MB
# app slot, the same on all seven boards. That WHX is not released from this
# repository: the pico-bootLoader repo builds those variants, WHX included, and
# its updateAll converts doom1.whx at that offset itself.
WHX_ADDR_STANDALONE=0x10080000
