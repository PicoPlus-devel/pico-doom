# Changelog

Add a section per release, headed with the tag. `release-notes.sh` extracts the
section matching the tag being released and puts it in the GitHub release body,
so keep the heading text exactly the tag name.

## v0.2

### Added

- **Options → Bootsel Mode** — resets the board straight into the RP2350's ROM
  bootloader, so it reappears as the `RPI-RP2` drive ready for a new `.uf2`.
  No more unplugging the board and holding BOOTSEL down to reflash it. There is
  no confirmation prompt, which is why it sits at the bottom of the Options
  menu, furthest from where the cursor starts. Settings are flushed to the SD
  card on the way out, so anything just changed in the menu survives the
  reflash. This is a different destination from *Quit Game*, which still
  returns to the pico-bootLoader picker, or to the DOS prompt when standalone.

### Build

- `tools/make_nespad_vpatch.py` is now `tools/make_menu_vpatches.py` and emits
  every firmware-resident menu graphic rather than just the one. Labels are
  still cut from Doom's own menu font in `DOOM.WAD`; the `B` of "Bootsel Mode"
  is spliced together from `P` and `D`, the font having no `B` of its own.

## v0.1

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
- An optional four-button **NES pad layout** (Options → NES PAD) for playing
  with a plain NES controller, on the legacy ports or over USB: A fires, B
  strafes, Start opens doors, Select cycles weapons, Select+Start toggles the
  menu, and the marine always runs
- SD-card variants (`doom_tiny_<board>_full.uf2`) that load a full registered
  WAD from `/roms/doom/doom.whd` into PSRAM at boot
- Saved games on the SD card (`/SAVES/doomsav0.dsg` … `doomsav5.dsg`), on every
  board and every build. Nothing is written to flash, so there is no
  sector-packing allocator and no "not enough space to save the game" limit
- Settings persist across power cycles too, in `/settings_DOOM.dat`: SFX and
  music volume, messages, screen size, mouse sensitivity, gamma, the FPS
  overlay and the NES pad layout. Written on leaving the menus and on quit
- A missing card costs persistence only: the game runs, the load menu is empty,
  saving reports why, and a card inserted later is picked up within seconds
- Quit returns to the pico-bootLoader, or shows a DOS prompt standalone
- Missing or unusable game data (no WHX flashed, no SD card, no
  `/roms/doom/doom.whd`, wrong format) explains itself on screen instead of
  halting on a black screen

### Build

- The Pico SDK, pico-extras and Pico-PIO-USB are taken from `PICO_SDK_PATH`,
  `PICO_EXTRAS_PATH` and `PICO_PIO_USB_PATH` instead of `3rdparty/` submodules.
  tinyusb comes from the SDK's own `lib/tinyusb`.
