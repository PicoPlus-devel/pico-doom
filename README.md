# Doom on the Adafruit Fruit Jam

A port of **Doom** to the [Adafruit Fruit Jam](https://www.adafruit.com/product/6200)
(RP2350B). It runs the full shareware `DOOM1.WAD` from flash, drives an HDMI
display over HSTX, plays music and sound effects — over that same HDMI link *or*
through the onboard TLV320 DAC to speaker/headphones — and takes input from USB
keyboards, mice, and gamepads.

The port descends from Graham Sanderson's
[rp2040-doom](https://github.com/kilograham/rp2040-doom) — which fits Doom into
a Raspberry Pi Pico — which is in turn derived from
[Chocolate Doom](https://github.com/chocolate-doom/chocolate-doom). For the deep
technical story of the RP2xxx port (WAD compression, memory tricks, the
`whd_gen` tool) see [README.RP2040.md](README.RP2040.md); for the upstream engine
see [README-chocolate.md](README-chocolate.md).

## What you need

**Hardware**

* An Adafruit Fruit Jam (RP2350B) board.
* An HDMI display and cable.
* At least one USB input device — a keyboard, a mouse, and/or a supported
  gamepad (see [Controls](#controls)).
* Optional: headphones or a speaker (audio also comes out over HDMI).
* Optional: a microSD card, only needed for the
  [pico-bootLoader](#running-under-the-pico-bootloader) route.

**Build host** (Linux or macOS)

* `arm-none-eabi-gcc` — the CI builds with **13.2.Rel1**; other versions may
  work but binary size is tight, so mismatches can cause link failures.
* `cmake` and `make`.
* `picotool` is fetched and built automatically by the build script; the
  Pico SDK and pico-extras come in as git submodules.

## Quick start (standalone)

```bash
git submodule update --init
./fruitjam-build.sh
```

This produces two files:

* `build_fruitjam/src/doom_tiny.uf2` — the game.
* `build_fruitjam/src/doom1-whx.uf2` — the shareware WAD data.

Put the Fruit Jam into BOOTSEL mode and copy **both** `.uf2` files to it over
USB. The device reboots after the first file, so you may need to re-enter the
bootloader before copying the second one.

There is a single `doom_tiny.uf2` that supports **all** input devices (USB
keyboard, mouse, and gamepads). The old per-input variants
(`doom_tiny_usb`, `doom_tiny_nost`, `doom_tiny_nost_usb`) no longer exist. USB
input is handled by the
[pico_shared](https://github.com/fhoedemakers/pico_shared) HID driver, shared
with the pico-infonesPlus family of emulators.

## Running under the pico-bootLoader

To run Doom under the resident
[pico-bootLoader](https://github.com/fhoedemakers/pico-bootLoader) — the menu
that lives on an SD card and flashes/launches emulators — build with:

```bash
git submodule update --init
./fruitjam-build-forbootloader.sh
```

This links `doom_tiny` into the bootloader's application partition (and places
the WAD data further up in flash — see [Flash map](#flash-map)). Outputs land in
a separate tree so the two builds don't clobber each other:

* `build_bl_fruitjam/src/doom_tiny.uf2`
* `build_bl_fruitjam/src/doom1-whx.uf2`

**Do not** drag these onto the Fruit Jam over USB. Instead place both files on
the pico-bootLoader SD card under `/emu/<hwconfig>/` and let the menu flash them
(Doom's main image plus its WHX companion). Every board's script emits the WAD
under the fixed name `doom1-whx.uf2`, which is the name the bootloader looks up;
the per-config SD folders keep the different boards' copies from colliding. See
the pico-bootLoader README for the SD-card layout and `emulators.txt`.

### Quitting

Selecting *Quit Game* (or pressing F10 and confirming) does one of two things,
decided at runtime — the same binary behaves correctly either way:

* **Launched from the pico-bootLoader** — the ENDOOM screen is skipped, the quit
  sound is allowed to finish, and the board resets back into the emulator
  picker. Doom explicitly asks the loader to skip its resume jump (watchdog
  scratch register 7, the same handshake as `pico_shared`'s
  `Frens::rebootToBootloader()`), so you get the picker and not a relaunch.
* **Standalone** — the ENDOOM screen appears, followed by a fake DOS prompt.
  `DIR`, `CLS` and `CD` work; typing `DOOM` (or `DOOM.EXE`) and pressing Return
  restarts the game. This needs a USB keyboard.

The UART reports which path was taken. Detection is
`watchdog_hw->scratch[6] == 0xB007ED01`, set by the loader just before it jumps
to the application — a cold reset or a BOOTSEL flash clears it, so a
BOOTSEL-flashed image correctly counts as standalone.

The exit screen is a true 80×25 VGA text mode: 8×16 glyphs, the full CP437 set
and the 16-colour attribute byte including blink, rendered as 640×400 centred in
the 640×480 HDMI signal.

## Full game from SD card (`doom_tiny_full`)

The default build embeds the shareware episode in flash. The **full variant**
instead loads a complete registered/Ultimate `doom.wad` (all episodes, built
with `WHD_SUPER_TINY=0`) from the SD card into **PSRAM** at boot and plays it
from there — the in-game *New Game → Episode* menu then offers every episode,
and the Episode 3 bunny finale works. Save games still live in flash (in the
region the WHX used to occupy) and are keyed to the WAD, so saves made with a
different WAD are refused rather than corrupted.

1. Convert your registered/Ultimate `doom.wad` (input first, output second):

   ```bash
   build/src/whd_gen/whd_gen doom.wad doom.whd -no-super-tiny
   ```

   (See [Converting a different WAD](#converting-a-different-wad-whd_gen) for
   building `whd_gen`.)

2. Copy `doom.whd` to the SD card as `/roms/doom/doom.whd` (FAT32/exFAT).

3. Build and flash the game (standalone or bootloader — no WHX uf2 involved):

   ```bash
   ./fruitjam-build-full.sh                  # → build_full_fruitjam/src/doom_tiny_full.uf2
   ./fruitjam-build-full-forbootloader.sh    # → build_bl_full_fruitjam/src/doom_tiny_full.uf2
   ```

   Every board has the same script pair (`adafruitdvisd-build-full.sh`, …).

At boot the game initializes the PSRAM (QMI CS1, mapped at `0x11000000`),
mounts the SD card over SPI and copies the WHD into PSRAM (~2 s), logging
progress on the UART. Any failure — no PSRAM, no card, missing or non-WHD
file — panics with a descriptive UART message.

**PSRAM requirement per board:**

| Board | PSRAM |
|-------|-------|
| Fruit Jam (`fruitjam`) | onboard 8 MB |
| Pico 2 + DVI (`adafruitdvisd`) | fit a [Pimoroni Pico Plus 2](https://shop.pimoroni.com/products/pimoroni-pico-plus-2) (8 MB PSRAM on GPIO 47) instead of a stock Pico 2 |
| Murmulator M2 (`murmulatorm2`) | onboard (GPIO 8) |
| Feather RP2350 (`featherrp2350`) | external APS6404 wired to GPIO 8 |

## Flash map

The build scripts flash the WAD data (`doom1.whx`) to a fixed address that
depends on which build you ran. These values come from `TINY_WAD_ADDR` in
[fruitjam_cflags.h](fruitjam_cflags.h) and the two build scripts:

| Build | `doom_tiny` image | WHX (WAD) data |
|-------|-------------------|----------------|
| Standalone (`fruitjam-build.sh`) | `0x10000000` | `0x10080000` |
| Bootloader (`fruitjam-build-forbootloader.sh`) | `0x10080000` | `0x10400000` |

These addresses are for the Fruit Jam. The other boards share the standalone map
(WHX at `0x10080000`); their **bootloader** maps differ — the 4 MB Pico 2 boards
(`adafruitdvisd`, `murmulatorm2`) place the WHX at `0x10200000`. See
[Other supported boards](#other-supported-boards).

## Converting a different WAD (`whd_gen`)

`doom1.whx` (the compressed shareware WAD) is bundled in this repository, so you
don't need to generate anything for the default build. To run a different WAD you
convert it to the RP2350 WHX/WHD format with the `whd_gen` host tool.

`whd_gen` needs none of the game's runtime dependencies — no SDL, no `pico-sdk`,
just a C/C++ toolchain and `CMake`. Build only that target from a native build
directory:

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make whd_gen
```

The binary lands at `build/src/whd_gen/whd_gen`. `cmake ..` configures even
without the SDL2 packages installed — it just skips the desktop `chocolate-doom`
build. Use a **release** build: the debug build deliberately lowers sound-effect
encoding quality for speed.

Then convert a WAD (input first, output second):

```bash
# super-compressed WHX — needed to fit DOOM1.WAD on a 2 MB board:
build/src/whd_gen/whd_gen DOOM1.WAD doom1.whx

# larger WADs (Ultimate Doom, Doom II) on 8 MB boards — less compression:
build/src/whd_gen/whd_gen DOOM2.WAD doom2.whd -no-super-tiny
```

### Windows executables

[src/whd_gen/build_native.sh](src/whd_gen/build_native.sh) builds `whd_gen`
without CMake, and can cross-compile standalone Windows executables from
Linux/WSL with the MinGW-w64 toolchain:

```bash
sudo apt install g++-mingw-w64-x86-64   # or g++-mingw-w64-i686 for 32-bit
src/whd_gen/build_native.sh win64       # -> src/whd_gen/whd_gen-win64.exe
src/whd_gen/build_native.sh win32       # -> src/whd_gen/whd_gen-win32.exe
```

The `.exe` is statically linked: copy it to any Windows machine and run it from
a Command Prompt, no DLLs required. On Windows itself the same script also
works in an [MSYS2](https://www.msys2.org/) MinGW shell (`./build_native.sh`).

For the deeper story (WHX vs WHD, caveats about non-id WADs) see
[README.RP2040.md](README.RP2040.md#whd_gen).

## Other supported boards

Besides the Fruit Jam, three more RP2350 HSTX boards from the
[pico_shared](https://github.com/fhoedemakers/pico_shared) family are
supported. Each has its own pin header (`<tag>_cflags.h`) and script pair —
`<tag>-build.sh` (standalone) and `<tag>-build-forbootloader.sh`
(pico-bootLoader) — producing `build_<tag>/src/doom_tiny.uf2` and
`build_<tag>/src/doom1-whx.uf2` (bootloader builds use
`build_bl_<tag>/`).

| Tag | HW_CONFIG | Board | USB host | Audio | Legacy pads |
|-----|-----------|-------|----------|-------|-------------|
| `adafruitdvisd` | 2 | Pico 2 + Adafruit DVI Breakout + SD breakout (breadboard or PCB) | native USB (OTG adapter) | HDMI + optional PCM5100A on GPIO 26/27 | 2× NES/SNES |
| `murmulatorm2` | 13 | Murmulator M2 (Pico 2 module) | native USB (OTG adapter) | HDMI + PCM5100A | 2× NES/SNES |
| `featherrp2350` | 14 | Adafruit Feather RP2350 + TLV320DAC3100 breakout | PIO-USB on GPIO 24/25 (USB Host FeatherWing) | HDMI + TLV320 | — |

Notes:

* **USB stack** — the Fruit Jam and Feather run the USB host on Pico-PIO-USB;
  the other two use the RP2350's native USB controller (plug gamepads in via
  an OTG adapter). The transport is fixed at build time: the board header's
  `HAS_USBPIO` define and the script's `-DENABLE_PIO_USB=` value must agree.
* **Audio** — boards without the Fruit Jam's headphone-detect pin play audio
  on HDMI *and* the I2S DAC simultaneously instead of switching sinks.
* **NES/SNES controllers** — configs 2 and 13 poll two legacy controller
  ports through the vendored pico_shared `nespad` PIO driver; SNES pads are
  auto-detected. Mapping: B/A = fire, Y/B = run, SNES A = strafe, SNES X =
  use, SNES L/R = strafe left/right, Select/Start = previous/next weapon.
* **Flash size for bootloader builds** — the bootloader map is sized per board.
  Fruit Jam (16 MB) and Feather (8 MB) place the WHX at `0x10400000`. The two
  Pico 2 boards (`adafruitdvisd`, `murmulatorm2`) cap the map to the 4 MB chip:
  a 1.5 MB app slot at `0x10080000` and the WHX at `0x10200000` (ending
  ~`0x103B7900`, comfortably under the 4 MB mark), so their bootloader builds
  run on a **genuine 4 MB Pico 2** as well as larger clone modules. Standalone
  builds (WHX at `0x10080000`) fit every board.
* **Video jitter trade-off** — on the native-USB boards `clk_hstx` is derived
  from PLL_SYS (PLL_USB must stay at 48 MHz for the USB controller), so very
  strict HDMI sinks may show occasional sparkles; the PIO-USB boards keep the
  dedicated 126 MHz PLL_USB HSTX clock.

## Hardware and I/O

Pin assignments live in [fruitjam_cflags.h](fruitjam_cflags.h).

* **Video** — HDMI/DVI output over HSTX on GPIO 13/15/17/19
  (`GPIOHSTXCK`/`D0`/`D1`/`D2`). Rendered at Doom's classic resolution and
  upscaled by the [pico_shared](https://github.com/fhoedemakers/pico_shared)
  `pico_hdmi` driver.
* **Audio** — soundtrack and effects over I2S to the onboard **TLV320DAC3100**
  codec (speaker and headphone), plus HDMI data-island audio at 48 kHz. The
  headphone jack is auto-detected by the driver.
* **Input** — USB host (keyboard, mouse, gamepad). Hot-plug is not reliable:
  plug your devices in first, then reset.
* **Debug** — UART on GPIO 44 (TX) / 45 (RX), 115200 baud.
* **Clocking** — the RP2350 is overclocked to **378 MHz at 1.60 V**. The
  extra CPU headroom fixes the in-game audio slowdown.

Hold a button at power-on to disable audio:

* **btn3** — disable music.
* **btn2** — disable music *and* sound effects.

## Controls

The input mapping is fixed at build time.

### Keyboard

Any "boot keyboard" should work.

| Key | Action |
|-----|--------|
| **W A S D** | Move (not the arrow keys!) |
| **Q E** | Turn left / right |
| **R F** | Cycle weapons |
| **Ctrl** | Attack |
| **Space** | Open / activate |
| **Shift** | Run |

### Mouse

Any "boot mouse" should work.

| Input | Action |
|-------|--------|
| Move X | Turn |
| Move Y | Forward / back |
| Left button | Attack |
| Right button | Hold to strafe with X movement; also open / activate |
| Wheel | Should switch next/previous weapon (didn't work on the Logitech wheel mice tested) |

### Gamepad

Supported controllers:

* Original SNES/NES gamepads.
* SNES/NES "MantaPad" clones, e.g. [Adafruit #6285](https://www.adafruit.com/product/6285)
* Sony DualShock 4, DualSense, PlayStation Classic.
* Xbox 360 / One / Series (XInput), and 8BitDo pads in XInput mode.
* Sega Genesis / Mega Drive Mini, Retro-bit MD Arcade Pad.

Buttons use **SNES naming**. On PlayStation pads: triangle = X, square = Y. On
Xbox pads: Y = X, X = Y.

| Control | Action |
|---------|--------|
| D-pad / left stick | Move and turn |
| Shoulder buttons | Strafe |
| **X** | Fire |
| **Y** | Open / activate |
| **A** | Hold to turn D-pad rotation into strafing |
| **B** | Run |
| **Select / Start** | Previous / next weapon |

## Known issues

* USB hot-plug is not reliable: plug your devices in first, then reset.

**Would be nice:** `PICO_NET` support for deathmatch.
