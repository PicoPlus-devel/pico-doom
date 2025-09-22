# Adafruit Fruit Jam (work in progress)

 * `git submodule update --init`
 * `sh fruitjam_build.sh`

You will get binary files including `build_fruitjam/src/doom_tiny.uf2`. and `build_fruitjam/src/doom1-whx-for-fruitjam.uf2`.
Copy those two uf2s to fruit jam.
You may need to re-enter the bootloader after copying the first file.

Note: When generating your own whx files, fruit jam ALWAYS uses the offset of 0x10080000.

You should get:
 * Debug UART output on pin "A4"
 * video data on pin 8, sync on 6/7 (I expected data on 9/10 as well but there's not: It's supposed to be 1-bit RGB)
 * Audio (soundtrack + FX) on I2S
 * USB input (keyboard, mouse, joypad) on USB ports. Hot plug is not reliable, please plug in devices and then reset.

The input mapping is fixed at build time:
 * Keyboard (any "boot keyboard" should do)
   * WASD movement
   * QE strafe
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

 * Joypad (hard coded for https://www.adafruit.com/product/6285)
   * dpad: forward/back/rotation
   * shoulder buttons: strafe
   * x: fire
   * a/y: open/activate
   * b: run
   * select/start: cycle weapons

When starting, hold down:
 * FruitJam "btn2" to disable music
 * FruitJam "btn3" to disable music & sfx

TODO:
 * Fix status bar

Would be nice:
 * combined UF2 files
 * PICO\_NET
