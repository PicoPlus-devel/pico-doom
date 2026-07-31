# Changelog

Add a section per release, headed with the tag. `release-notes.sh` extracts the
section matching the tag being released and puts it in the GitHub release body,
so keep the heading text exactly the tag name.

## v1.0

First release of pico-doom as an independent repository (previously a fork of
rp2040-doom / fruitjam-doom). Chocolate Doom for RP2350 boards with HSTX video.

### Supported boards

- Adafruit Fruit Jam
- Pico 2 / Pico Plus 2 + Adafruit DVI breakout
- Murmulator M2 (RP2350)
- Adafruit Feather RP2350 + TLV320 breakout

### Features

- HSTX DVI/HDMI video output, with HDMI audio on the TLV320-equipped boards
- I2S audio via the TLV320DAC3100 or a PCM5100A DAC
- USB HID keyboard and gamepad support (native USB or PIO-USB, per board), with
  XInput pads (Xbox 360/One/Series, 8BitDo in X-mode)
- NES/SNES gamepads on the boards that have the ports
- Gamepad control in the menus and on the exit screen; Start opens the menu
- SD-card variants (`doom_tiny_<board>_full.uf2`) that load a full registered
  WAD from `/roms/doom/doom.whd` into PSRAM at boot
- Quit returns to the pico-bootLoader, or shows a DOS prompt standalone
- Missing or unusable game data (no WHX flashed, no SD card, no
  `/roms/doom/doom.whd`, wrong format) explains itself on screen instead of
  halting on a black screen

### Build

- The Pico SDK, pico-extras and Pico-PIO-USB are taken from `PICO_SDK_PATH`,
  `PICO_EXTRAS_PATH` and `PICO_PIO_USB_PATH` instead of `3rdparty/` submodules.
  tinyusb comes from the SDK's own `lib/tinyusb`.
