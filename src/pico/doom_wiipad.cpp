//
// Copyright(C) 2026 Frank Hoedemakers
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Wii-extension pad glue - see doom_wiipad.h for what this adds on top of
//	the bare pico_shared driver.
//

#include "doom_wiipad.h"

#if PICO_ON_DEVICE && WII_PIN_SDA >= 0 && WII_PIN_SCL >= 0

#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"

#include "wiipad.h"
#include "i2c_bus_recovery.h"
#include "audio_i2s.h"   // PICO_AUDIO_I2S_DRIVER_TLV320

#ifndef DOOM_AUDIO_I2S_DRIVER
#define DOOM_AUDIO_I2S_DRIVER PICO_AUDIO_I2S_DRIVER_TLV320
#endif

// The TLV320 boards (Fruit Jam, Feather) put the codec on the very same
// SDA/SCL as the Wii extension port, and the codec has a reset pin we can use
// to keep it out of the way while the pad is brought up. The PCM5100A boards
// (Murmulator M2) have the port to themselves.
#if DOOM_AUDIO_I2S_DRIVER == PICO_AUDIO_I2S_DRIVER_TLV320 && \
    defined(PICO_AUDIO_I2S_RESET_PIN) && PICO_AUDIO_I2S_RESET_PIN >= 0
#define WIIPAD_SHARES_DAC_BUS 1
#else
#define WIIPAD_SHARES_DAC_BUS 0
#endif

namespace
{
    // wiipad_read() is blocking and not cheap: a register-pointer write, a
    // 200 us settle sleep and a 6-byte read, ~400 us in all. Doom runs at 35
    // tics/s, so there is nothing to gain from reading faster than this, and
    // there is plenty to lose: pollGamePads() is also called in a tight loop
    // by exit_screen_pump() (i_system.c), which would otherwise spend the
    // whole exit screen inside the I2C driver. Upstream's rule is "once per
    // rendered frame"; 10 ms is the same order and keeps the added input
    // latency under half a tic.
    constexpr uint64_t MIN_POLL_US = 10000;

    // A pad plugged in after boot (or one that was not ready in time - the
    // SNES-Classic pads are slow to wake) gets picked up by retrying the
    // handshake. A failed probe NACKs out in ~300 us so this is close to free;
    // a *successful* one costs ~200 ms once, from the two sleep_ms(100) inside
    // wiipad_begin(). One hitch at plug-in time is a fair price for hot-plug.
    constexpr uint64_t RECONNECT_US = 1000000;
}

void doom_wiipad_init(void)
{
#if WIIPAD_SHARES_DAC_BUS
    // Mirrors initVintageControllers() in pico-infonesPlus
    // (pico_shared/FrensHelpers.cpp). An uninitialized SNES-Classic pad
    // attached at power-on wedges the shared bus outright, and every DAC
    // transaction then times out. Clear the bus, give an attached pad its
    // extension-init so it goes quiet, and clear again before handing over.
    // The DAC is held in reset meanwhile so it cannot observe the pad
    // traffic; tlv320_hardware_reset() releases it later.
    gpio_init(PICO_AUDIO_I2S_RESET_PIN);
    gpio_put(PICO_AUDIO_I2S_RESET_PIN, 0);
    gpio_set_dir(PICO_AUDIO_I2S_RESET_PIN, GPIO_OUT);
    i2c_bus_clear(WII_PIN_SDA, WII_PIN_SCL, "pad-preinit");
    wiipad_begin(); // fast NACK, no delays, when no pad is attached
    i2c_bus_clear(WII_PIN_SDA, WII_PIN_SCL, "pre-DAC");
#else
    wiipad_begin();
#endif
}

uint16_t doom_wiipad_read(void)
{
    static uint16_t last = 0;
    static uint64_t last_poll = 0;

    const uint64_t now = time_us_64();
    if (now - last_poll < MIN_POLL_US)
        return last;
    last_poll = now;

    if (!wiipad_is_connected())
    {
        static uint64_t last_try = 0;
        last = 0;
        if (now - last_try < RECONNECT_US)
            return 0;
        last_try = now;
        // Re-running i2c_init() on a bus the codec shares is safe: the only
        // other user is doom_poll_headphone(), and it runs on core0 from the
        // same game loop as this, so the two can never interleave.
        wiipad_begin();
        if (!wiipad_is_connected())
            return 0;
    }

    last = wiipad_read();
    return last;
}

void doom_wiipad_shutdown(void)
{
    // Only when a pad actually answered: wiipad_end() deinits the I2C
    // instance, which on the TLV320 boards is the codec's too.
    if (wiipad_is_connected())
        wiipad_end();
}

#endif // PICO_ON_DEVICE && WII_PIN_SDA >= 0 && WII_PIN_SCL >= 0
