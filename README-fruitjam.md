# Adafruit Fruit Jam (work in progress)

 * `git submodule update --init`
 * `sh fruitjam_build.sh`
 * `picotool uf2 convert doom1.whx -t bin doom1-whx-for-fruitjam.uf2 -o 0x10080000 --family data`

You will get binary files including `build_fruitjam/src/doom_tiny.uf2`.

Copy the uf2s (I copied `doom1-whx-for-fruitjam.uf2` first then `build_fruitjam/src/doom_tiny.uf2`) to fruit jam.
You may need to re-enter the bootloader after copying the first file.

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
