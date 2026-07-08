# Adafruit Fruit Jam (work in progress)

 * `git submodule update --init`
 * `sh fruitjam-build.sh`

You will get binary files including `build_fruitjam/src/doom_tiny.uf2`. and `build_fruitjam/src/doom1-whx-for-fruitjam.uf2`.
Copy those two uf2s to fruit jam.
You may need to re-enter the bootloader after copying the first file.

## Building for the pico-bootLoader

To run doom under the resident
[pico-bootLoader](https://github.com/fhoedemakers/pico-bootLoader) — the menu
that lives on the SD card and flashes/launches emulators — build with:

 * `git submodule update --init`
 * `sh fruitjam-build-forbootloader.sh`

Unlike the standalone build above, this links `doom_tiny` into the bootloader's
application partition at `0x10080000` (not `0x10000000`) and places the WHX data
at `0x10400000`. Outputs land in a separate tree so the two builds don't clobber
each other:

 * `build_bl_fruitjam/src/doom_tiny.uf2`
 * `build_bl_fruitjam/src/doom1-whx-for-fruitjam.uf2`

Do **not** drag these onto the Fruit Jam over USB. Instead place both on the
pico-bootLoader SD card under `/emu/<hwconfig>/` and let the menu flash them
(doom's main image plus its WHX companion). See the pico-bootLoader README for
the SD-card layout and `emulators.txt`.

There is a single `doom_tiny.uf2` that supports all input devices (USB keyboard,
mouse and gamepads); the old per-input variants (`doom_tiny_usb`, `doom_tiny_nost`,
`doom_tiny_nost_usb`) no longer exist. USB input is handled by the
[pico_shared](https://github.com/fhoedemakers/pico_shared) HID driver, shared
with the pico-infonesPlus family of emulators.

Note: When generating your own whx files for the standalone build, fruit jam
ALWAYS uses the offset of 0x10080000. (The pico-bootLoader build places the WHX
at 0x10400000 instead — see "Building for the pico-bootLoader" above.)

You should get:
 * Debug UART output on pin "A4"
 * video data on pin 8, sync on 6/7 (I expected data on 9/10 as well but there's not: It's supposed to be 1-bit RGB)
 * Audio (soundtrack + FX) on I2S
 * USB input (keyboard, mouse, joypad) on USB ports. Hot plug is not reliable, please plug in devices and then reset.

The input mapping is fixed at build time:
 * Keyboard (any "boot keyboard" should do)
   * WASD movement -- NOT the arrow keys!
   * QE rotation
   * RF cycle weapons
   * ctrl: attach
   * space: open/activate
   * run: shift

 * Mouse: (any "boot mouse" should do)
   * X: rotation
   * Y: forward/back
   * Left button: attack
   * Right button: toggle X movement to strafe
   * Right button: open/activate
   * Wheel did not work on the two USB wheel mouse I tested (logitech) but should switch next/previous weapon

 * Joypad — supported controllers:
   * SNES/NES "MantaPad" clones, e.g. https://www.adafruit.com/product/6285 (on the NES variant, press Y once to switch it to SNES mode)
   * Sony DualShock 4, DualSense, PlayStation Classic
   * Xbox 360 / One / Series (XInput), 8BitDo pads in XInput mode
   * Sega Genesis/Mega Drive Mini, Retro-bit MD Arcade Pad

   Buttons (SNES naming; on PlayStation pads triangle=X, square=Y; on Xbox pads Y=X, X=Y):
   * dpad (and left stick): forward/back/rotation
   * shoulder buttons: strafe
   * x: fire
   * y: open/activate
   * a: hold to turn dpad rotation into strafing
   * b: run
   * select/start: previous/next weapon

When starting, hold down:
 * FruitJam "btn3" to disable music
 * FruitJam "btn2" to disable music & sfx

Known problems:
 * Status bar flickers
 * End of level stats are invisible

Would be nice:
 * Fix "overlays", which is the underlying cause of the two known problems
 * PICO\_NET for deathmatch
