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

TODO:
 * HSTX video
 * Pico-PIO-USB

Would be nice:
 * combined UF2 files
 * PICO\_NET
