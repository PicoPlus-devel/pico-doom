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

> **Status: work in progress.** It's very playable, but a few rendering issues
> remain — see [Known issues](#known-issues).

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
* `build_fruitjam/src/doom1-whx-for-fruitjam.uf2` — the shareware WAD data.

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
* `build_bl_fruitjam/src/doom1-whx-for-fruitjam.uf2`

**Do not** drag these onto the Fruit Jam over USB. Instead place both files on
the pico-bootLoader SD card under `/emu/<hwconfig>/` and let the menu flash them
(Doom's main image plus its WHX companion). See the pico-bootLoader README for
the SD-card layout and `emulators.txt`.

## Flash map

The build scripts flash the WAD data (`doom1.whx`) to a fixed address that
depends on which build you ran. These values come from `TINY_WAD_ADDR` in
[fruitjam_cflags.h](fruitjam_cflags.h) and the two build scripts:

| Build | `doom_tiny` image | WHX (WAD) data |
|-------|-------------------|----------------|
| Standalone (`fruitjam-build.sh`) | `0x10000000` | `0x10080000` |
| Bootloader (`fruitjam-build-forbootloader.sh`) | `0x10080000` | `0x10400000` |

`doom1.whx` (the compressed shareware WAD) is bundled in this repository, so you
don't need to generate anything for the default build. To convert a different
WAD, use the `whd_gen` tool — see [README.RP2040.md](README.RP2040.md#whd_gen).

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
* **Debug** — UART on GPIO 44 (TX) / 45 (RX).

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

* SNES/NES "MantaPad" clones, e.g. [Adafruit #6285](https://www.adafruit.com/product/6285)
  (on the NES variant, press **Y** once to switch it to SNES mode).
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

* The status bar flickers.
* End-of-level stats are invisible.

Both are caused by the way "overlays" are currently handled; fixing overlays is
the underlying fix for both.

**Would be nice:** `PICO_NET` support for deathmatch.

## Building on a host and licensing

You can also build a native `chocolate-doom` (useful for verification) or an
SDL host build of RP2040 Doom — see [README.RP2040.md](README.RP2040.md) for
both, plus details on the `whd_gen` WAD converter.

Code derived from Chocolate Doom keeps its existing license (generally GPLv2);
new RP2040 Doom code is BSD-3. See [COPYING.md](COPYING.md) for the full terms.
