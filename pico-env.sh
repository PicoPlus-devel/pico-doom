# Resolve the Pico toolchain from the environment.
#
# Sourced (not executed) by every <board>-build*.sh script, so the same checks
# run for all 16 build configurations without being duplicated 16 times:
#
#     TAG=fruitjam
#     NEED_PIO_USB=1        # only the boards whose USB host runs on PIO
#     . ./pico-env.sh
#
# The Pico SDK, pico-extras and Pico-PIO-USB used to be git submodules under
# 3rdparty/. They are now external checkouts pointed at by environment
# variables, so a CI runner and a local tree share one provisioned toolchain:
#
#     export PICO_SDK_PATH=/path/to/pico-sdk
#     export PICO_EXTRAS_PATH=/path/to/pico-extras
#     export PICO_PIO_USB_PATH=/path/to/Pico-PIO-USB     # PIO-USB boards only
#
# tinyusb is NOT one of them: it comes from the SDK's own lib/tinyusb submodule
# (SDK 2.2.0 ships 0.18.0, which has the RP2350 fixes and board definitions).
#
# Paths are made absolute here on purpose. pico_sdk_import.cmake resolves a
# relative PICO_SDK_PATH against CMAKE_BINARY_DIR rather than the source dir, so
# a relative value only ever worked by accident of the build directory sitting
# inside the source tree.

pico_env_die() {
    echo "" >&2
    echo "ERROR: $*" >&2
    echo "" >&2
    echo "Set the toolchain paths first, for example:" >&2
    echo "    export PICO_SDK_PATH=\$HOME/pico/pico-sdk" >&2
    echo "    export PICO_EXTRAS_PATH=\$HOME/pico/pico-extras" >&2
    echo "    export PICO_PIO_USB_PATH=\$HOME/pico/Pico-PIO-USB" >&2
    echo "See README.md, section \"Building\"." >&2
    exit 1
}

# --- Pico SDK ---------------------------------------------------------------
[ -n "$PICO_SDK_PATH" ] || pico_env_die "PICO_SDK_PATH is not set."
[ -d "$PICO_SDK_PATH" ] || pico_env_die "PICO_SDK_PATH=$PICO_SDK_PATH is not a directory."
PICO_SDK_PATH=$(cd "$PICO_SDK_PATH" && pwd) || exit 1
[ -f "$PICO_SDK_PATH/pico_sdk_init.cmake" ] || \
    pico_env_die "PICO_SDK_PATH=$PICO_SDK_PATH has no pico_sdk_init.cmake -- not a Pico SDK checkout."

# tinyusb lives in the SDK and must have been checked out: the USB host build
# (and the tinyusb_pico_pio_usb target the PIO boards link) comes from there.
[ -f "$PICO_SDK_PATH/lib/tinyusb/hw/bsp/family_support.cmake" ] || \
    pico_env_die "$PICO_SDK_PATH/lib/tinyusb is empty.
Initialise the SDK's own submodules:
    git -C $PICO_SDK_PATH submodule update --init"

export PICO_SDK_PATH

# --- pico-extras (pico_audio_headers, pico_scanvideo_dpi) -------------------
[ -n "$PICO_EXTRAS_PATH" ] || pico_env_die "PICO_EXTRAS_PATH is not set."
[ -d "$PICO_EXTRAS_PATH" ] || pico_env_die "PICO_EXTRAS_PATH=$PICO_EXTRAS_PATH is not a directory."
PICO_EXTRAS_PATH=$(cd "$PICO_EXTRAS_PATH" && pwd) || exit 1
[ -f "$PICO_EXTRAS_PATH/external/pico_extras_import.cmake" ] || \
    pico_env_die "PICO_EXTRAS_PATH=$PICO_EXTRAS_PATH does not look like a pico-extras checkout."
export PICO_EXTRAS_PATH

# --- Pico-PIO-USB (only the boards that drive USB host over PIO) ------------
# The native-USB boards (adafruitdvisd, murmulatorm2) pass -DENABLE_PIO_USB=0
# and never link tinyusb_pico_pio_usb, so they do not need this at all.
if [ "${NEED_PIO_USB:-0}" = "1" ]; then
    [ -n "$PICO_PIO_USB_PATH" ] || \
        pico_env_die "PICO_PIO_USB_PATH is not set, but this board drives USB host over PIO (HAS_USBPIO)."
    [ -d "$PICO_PIO_USB_PATH" ] || \
        pico_env_die "PICO_PIO_USB_PATH=$PICO_PIO_USB_PATH is not a directory."
    PICO_PIO_USB_PATH=$(cd "$PICO_PIO_USB_PATH" && pwd) || exit 1
    [ -f "$PICO_PIO_USB_PATH/src/pio_usb.c" ] || \
        pico_env_die "PICO_PIO_USB_PATH=$PICO_PIO_USB_PATH has no src/pio_usb.c -- not a Pico-PIO-USB checkout."
    export PICO_PIO_USB_PATH
fi

echo "Pico SDK      : $PICO_SDK_PATH"
echo "pico-extras   : $PICO_EXTRAS_PATH"
if [ "${NEED_PIO_USB:-0}" = "1" ]; then
    echo "Pico-PIO-USB  : $PICO_PIO_USB_PATH"
fi
