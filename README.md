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
* A microSD card (FAT32/exFAT) for [saved games and settings](#saved-games-and-settings).
  The game runs without one; you just cannot save anything. It is also what the
  [pico-bootLoader](#running-under-the-pico-bootloader) route and the
  [full-game build](#full-game-from-sd-card-doom_tiny_full) need.

**Build host** (Linux or macOS)

* `arm-none-eabi-gcc` — the CI builds with **13.2.Rel1**; other versions may
  work but binary size is tight, so mismatches can cause link failures.
* `cmake` and `make`.
* `picotool` **2.2.0 or newer** on your `PATH` (the build falls back to fetching
  and building it, but a system install is much faster).
* The Pico SDK, pico-extras and Pico-PIO-USB, pointed at by environment
  variables — they are *not* submodules of this repository:

  ```bash
  export PICO_SDK_PATH=$HOME/pico/pico-sdk
  export PICO_EXTRAS_PATH=$HOME/pico/pico-extras
  export PICO_PIO_USB_PATH=$HOME/pico/Pico-PIO-USB   # PIO-USB boards only
  ```

  The SDK must have its own submodules checked out
  (`git -C $PICO_SDK_PATH submodule update --init`): tinyusb comes from the SDK's
  `lib/tinyusb`, not from this repository. `PICO_PIO_USB_PATH` is only needed by
  the boards whose USB host runs on PIO — the Fruit Jam and the Feather RP2350;
  the Pico 2 and Murmulator M2 builds use native USB and pass
  `-DENABLE_PIO_USB=0`. Every build script validates these paths up front via
  [pico-env.sh](pico-env.sh) and stops with a clear message if one is wrong.

## Quick start (standalone)

```bash
git submodule update --init      # tusb_xinput + the shared driver library
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
* **Standalone** — the ENDOOM screen appears with a fake DOS prompt already on
  it. With a keyboard, `DIR`, `CLS` and `CD` work, and typing `DOOM` (or
  `DOOM.EXE`) and pressing Return restarts the game. With only a gamepad,
  **Start** does the same thing.

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
and the Episode 3 bunny finale works. [Saved games](#saved-games-and-settings)
land on the same card and are keyed to the WAD, so saves made with a different
WAD are refused rather than corrupted.

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

## Saved games and settings

Both live on the microSD card, on every board and every build:

| What | Where |
|------|-------|
| Saved games | `/SAVES/doomsav0.dsg` … `/SAVES/doomsav5.dsg` (the six menu slots) |
| Settings | `/settings_DOOM.dat` |

The `/SAVES` directory is created on the first save. The naming follows
[pico-infonesPlus](https://github.com/fhoedemakers/pico-infonesPlus), so one card
can hold the saves of both without colliding.

Saved games are the compressed format the port has always used, and are still
keyed to the WAD — a save made with a different WAD is refused rather than
loaded as garbage.

The settings file covers everything this build can actually change: SFX and
music volume, messages on/off, screen size, mouse sensitivity, gamma (**F11**),
the FPS overlay (**\\**) and the NES pad layout. Changes are written when you
leave the menus and when you quit, so gamma or screen size changed with a hotkey
mid-game is stored the next time you open and close the menu. A missing or
damaged file is ignored and the built-in defaults are used; a file from an older
version is read for the settings it does have.

Without a card the game runs normally — the load menu is empty, saving reports
the reason, and settings reset at power-off. Insert a card and it starts working
within a few seconds; no reset needed.

## Flash map

Nothing is written to flash any more, so the map is entirely static. The build
scripts flash the WAD data (`doom1.whx`) to a fixed address that
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

### Download a prebuilt `whd_gen`

Every [release](../../releases) attaches ready-to-run `whd_gen` binaries. They are
statically linked — nothing to install, no DLLs, no glibc version to match:

| File | Platform |
|---|---|
| `whd_gen-win64.exe` | 64-bit Windows |
| `whd_gen-linux-x64` | x86_64 Linux |
| `whd_gen-linux-arm64` | arm64 Linux (Raspberry Pi OS 64-bit) |

On Linux, make it executable first (`chmod +x whd_gen-linux-*`). The two
conversions that matter, and which firmware each one pairs with:

```bash
# WAD lives in flash — pairs with doom_tiny_<board>.uf2
whd_gen DOOM1.WAD doom1.whx

# WAD lives on the SD card as /roms/doom/doom.whd — pairs with doom_tiny_<board>_full.uf2
whd_gen doom.wad doom.whd -no-super-tiny
```

There is no prebuilt binary for 32-bit Windows or macOS; build from source below
(`src/whd_gen/build_native.sh` needs only a C/C++ compiler). Output is
byte-identical whichever platform produced the binary.

### Build `whd_gen` from source

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

### Cross-compiling `whd_gen` (Windows, arm64)

[src/whd_gen/build_native.sh](src/whd_gen/build_native.sh) builds `whd_gen`
without CMake, one target per invocation:

```bash
src/whd_gen/build_native.sh                # -> src/whd_gen/whd_gen (dynamic, for dev)
src/whd_gen/build_native.sh linux-x64      # -> whd_gen-linux-x64    (static)
src/whd_gen/build_native.sh linux-arm64    # -> whd_gen-linux-arm64  (static)
src/whd_gen/build_native.sh win64          # -> whd_gen-win64.exe    (static)
src/whd_gen/build_native.sh win32          # -> whd_gen-win32.exe    (static)
```

The Windows targets need MinGW-w64 (`sudo apt install g++-mingw-w64-x86-64`, or
`g++-mingw-w64-i686` for 32-bit). The `.exe` is statically linked: copy it to any
Windows machine and run it from a Command Prompt, no DLLs required. On Windows
itself the same script works in an [MSYS2](https://www.msys2.org/) MinGW shell.

For `linux-arm64` you need an aarch64 cross-compiler. **Do not
`apt install g++-aarch64-linux-gnu`** — on Ubuntu 24.04 that package
Breaks/Replaces `gcc-multilib`/`g++-multilib`, so apt removes them and any `-m32`
build on the machine stops working. Use a standalone toolchain tarball, such as
the ARM GNU Toolchain `aarch64-none-linux-gnu` build, and point the script at it
(note the `-none-` in the prefix):

```bash
CC=/opt/arm-gnu/bin/aarch64-none-linux-gnu-gcc \
CXX=/opt/arm-gnu/bin/aarch64-none-linux-gnu-g++ \
  src/whd_gen/build_native.sh linux-arm64
```

For the deeper story (WHX vs WHD, caveats about non-id WADs) see
[README.RP2040.md](README.RP2040.md#whd_gen).

## Cutting a release

Releases are built by
[.github/workflows/BuildAndRelease.yml](.github/workflows/BuildAndRelease.yml) on
a self-hosted runner (a Raspberry Pi 5) when a `v*` tag is pushed. The workflow is
a thin wrapper: everything it does is `./buildAll.sh` and `./release-notes.sh`, so
running those two scripts locally *is* testing the pipeline.

```bash
./buildAll.sh                    # fills releases/ -- exactly what CI runs
./release-notes.sh v1.0          # writes release-notes.md, the release body
```

`buildAll.sh` builds the standalone and SD-card variant for all four boards,
converts `doom1.whx` at the standalone flash offset, and copies the prebuilt
`whd_gen` binaries in — 12 files in all. The pico-bootLoader variants are **not**
released here: they are built in the
[pico-bootLoader](https://github.com/fhoedemakers/pico-bootLoader) repository,
which invokes `<board>-build-forbootloader.sh` itself and produces its own WHX at
the offset matching each board's flash map.

The runner needs `PICO_SDK_PATH`, `PICO_EXTRAS_PATH` and `PICO_PIO_USB_PATH` (set
in the workflow to `/datalocal/pico/...`), plus `picotool` ≥ 2.2.0 on `PATH`.

### Refreshing the prebuilt `whd_gen` binaries

The runner is ARM64 and never compiles `whd_gen`; it only copies
`prebuilt/whd_gen/*` into `releases/`. Those binaries are cross-built on an
x86_64 Linux box and committed:

```bash
./update-prebuilt-whd_gen.sh     # linux-x64, linux-arm64, win64
git add prebuilt/whd_gen && git commit
```

Do this whenever anything `whd_gen` compiles changes (`src/whd_gen/`,
`src/tiny_huff.c`, `src/musx_decoder.c`, `src/image_decoder.c`, `src/adpcm-xq/`).
`buildAll.sh` compares the commit recorded in `prebuilt/whd_gen/VERSIONS.txt`
against those paths and prints a warning if the binaries have fallen behind — a
warning rather than an error, so an unrelated commit cannot block a release.

### Steps

1. Add a `## vX.Y` section to [CHANGELOG.md](CHANGELOG.md) — the heading text
   must be exactly the tag, since `release-notes.sh` extracts that section.
2. Commit, then dry-run on the runner without publishing anything:
   `gh workflow run BuildAndRelease.yml` (leave the `tag` input empty).
3. `git tag vX.Y && git push origin vX.Y`.

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
  auto-detected. They use the same layout as the USB pads (see
  [Gamepad](#gamepad) below), including the optional NES pad layout.
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
| **X** | Fire |
| **Y** | Open / activate |
| **A** | Hold to turn D-pad rotation into strafing |
| **B** | Run |
| **L** / **Select** | Previous weapon |
| **R** | Next weapon |
| **Start** | Open / close the menu |

In menus, **A** or **X** selects and **B** or **Y** goes back, so the button you
reach for first works either way. **Start** opens and closes the menu, so a
keyboard is no longer needed to get into the options.

The shoulder buttons cycle weapons rather than strafing directly; strafe by
holding **A** and pushing left/right, which is how vanilla Doom's strafe button
works. A plain NES pad has no shoulders, so there Start opens the menu and
Select is the only weapon cycle — it still reaches every weapon.

#### NES pad layout

The layout above wants six buttons and two shoulders. A plain NES controller has
four buttons, and with the default layout it ends up without a Use button, so you
cannot open a door. **Options → Nes Pad** switches every pad — the legacy ports
and USB alike — to a layout built for those four buttons:

| Control | In game | In a menu |
|---------|---------|-----------|
| D-pad | Move and turn | Navigate |
| **A** | Fire | Select |
| **B** | Hold to strafe with the D-pad | Back |
| **Start** | Open / activate | — |
| **Select** | Next weapon | — |
| **Select + Start** | Open / close the menu | Open / close the menu |

There is no run button left, so the marine always runs while this is on. The
setting is stored on the SD card, and you can reach it with the pad alone: in the
default layout **Start** opens the menu, the D-pad moves and **A** selects.

It suits the NES-shelled MantaPad and an original NES controller; a SNES pad
keeps working, with **B**/**A** as NES A and **Y**/**X** as NES B.

## Known issues

* USB hot-plug is not reliable: plug your devices in first, then reset.

**Would be nice:** `PICO_NET` support for deathmatch.
