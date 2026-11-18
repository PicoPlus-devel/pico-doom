# Changelog

Add a section per release, headed with the tag. `release-notes.sh` extracts the
section matching the tag being released and puts it in the GitHub release body,
so keep the heading text exactly the tag name.

## Next release

### Changed

- Built with Pico SDK 2.3.1. This leaves a little more memory free, about 0.4 KB.

## v0.7

### Added

- **[Pico Computer 3](https://github.com/UKTailwind/Pico-Computer-3)** is now supported
  (`doom_tiny_picocomputer3.uf2`, and `doom_tiny_picocomputer3_full.uf2` for the full
  game from the SD card). Picture over HDMI, sound over HDMI and the audio jack at the
  same time, up to three USB devices (keyboard, mouse, gamepad) on the USB-A ports, and
  two NES/SNES controllers and a Wii Classic Controller on the I/O header. Not yet
  tested on this board, and the pin assignment may still change. Thanks to @javavi, who
  added the board to pico_shared.

## v0.6

### Added

- **DVI display mode**, for monitors and TVs that do not accept an HDMI signal. Choose
  HDMI or DVI under *Options → Display*; the picture switches at once and the choice is
  kept on the SD card. DVI carries no sound: it plays on the board's own audio output
  instead (on the Fruit Jam the speaker, or the headphones when plugged in). The
  Murmulator M2 now starts in DVI until another mode is chosen; choose HDMI for sound on
  the TV.
- **Waveshare RP2350-PiZero on the PCB by Liu Szuping** (@szuping) is now
  supported (`doom_tiny_pizeroliupcb.uf2`). Picture
  over the PCB's HDMI port, sound over HDMI and the audio jack at the same time,
  a USB keyboard, mouse or gamepad on the PiZero's PIO-USB port, and two NES/SNES
  controller ports. The full-game version needs a PSRAM chip soldered onto the
  PiZero. The PCB design is not yet available.
  **Known issue:** on this board, some TVs and monitors lose the picture for a few seconds
  at a time during play; others show it without interruption. This is a limitation of the
  RP2350-PiZero at 384 MHz, the clock the game needs.

## v0.5

### Changed

- The RP2350 now runs at 384 MHz instead of 378 MHz.

### Fixed

- **No more dots or lines in the picture** on a Pico 2 with the Adafruit DVI
  Breakout, the Murmulator M2 and the Olimex RP2040-PICO-PC. Some TVs and
  monitors showed small dots or short dotted lines. The picture is now clean,
  and USB controllers, keyboards and mice keep working. Thanks to @szuping for
  the hint that led to this fix.
- **USB on the Adafruit Fruit Jam and Feather RP2350:** USB controllers,
  keyboards and mice are now found at every start. On boards with an older
  RP2350 chip they were often not detected.

## v0.4

### Added

- **PicoNES PCB:** the README describes the PicoNES PCB for the Pico 2 board,
  by John Edgar Park: v2.6 with NES controller ports and v2.8 with SNES
  controller ports. The release includes the Gerber files of both.

### Changed

- **pico-bootLoader:** on the Fruit Jam and the Feather RP2350, Doom under the
  pico-bootLoader now keeps its WAD data at the same place in flash as on the
  other boards, so one WAD data file serves every board. The pico-bootLoader's
  updateAll installs it. Update Doom and its WAD data together.
- Links, and the download address on the error screen, now point to the
  PicoPlus-devel organisation on GitHub.

Nothing changes for boards flashed directly over USB.

## v0.3

### Added

- **Olimex RP2040-PICO-PC with a Raspberry Pi Pico 2** is now supported
  (`doom_tiny_olimexpicopc.uf2`). Picture and sound over HDMI, sound on the
  board's audio jack at the same time, USB keyboard, mouse and gamepads on the
  USB-A socket, and a NES/SNES pad on the UEXT connector. The full-game version
  needs a PSRAM chip fitted to the Pico 2 (GPIO 8).

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
