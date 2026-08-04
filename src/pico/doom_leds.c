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
//	Ports two pico-infonesPlus status-output features (see
//	../pico-infonesPlus/pico_shared):
//
//	  - the plain onboard LED heartbeat from Frens::blinkLed(), which the
//	    emulator drives with (frameCount / 60) & 1, i.e. 60 frames on and 60
//	    frames off;
//	  - vumeter.cpp's 5-pixel WS2812 audio VU meter, on the Fruit Jam's
//	    onboard NeoPixel strip.
//
//	The WS2812 program (ws2812.pio) is the stock Raspberry Pi one and is a
//	verbatim copy of pico_shared's. The meter *algorithm* is ported rather
//	than the file, because vumeter.cpp assumes it owns the audio path: it
//	takes one sample at a time and writes the PIO FIFO straight from the
//	emulator's mix loop. Doom's mixer (I_Pico_UpdateSound) runs on BOTH
//	cores with no lock, so that shape would race both the sample buffer and
//	the FIFO. Here the work is split:
//
//	  - doom_vu_add_chunk() runs on whichever core is mixing. It reduces the
//	    chunk to one number and publishes it with a single aligned 32-bit
//	    store, which is indivisible on Cortex-M33 (and RP2350 SRAM is
//	    uncached, so no barriers are needed). No lock, no float, no state
//	    that has to survive across calls.
//	  - doom_leds_frame() runs on core0 once per frame and owns everything
//	    else: the AGC, all floating point, and every FIFO write.
//
//	What is left is a benign race: the peak-hold compare-and-store can
//	interleave with the frame hook's read-and-clear, so a peak can be lost.
//	That is one dropped update in a meter that repaints 35-60 times a
//	second. The alternative -- accumulating a running sum and count across
//	calls -- would be a genuinely corrupting cross-core read-modify-write,
//	and sharing the float AGC state would be worse still: a torn
//	vu_dynamic_max feeds a divide and could peg or blank the meter for good.
//

#if PICO_ON_DEVICE

#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"

#include "doom_leds.h"

#if DOOM_VU_WS2812_PIN >= 0
#include "ws2812.pio.h"

// A pin above 31 is outside a single PIO's 32-pin window, so the SDK has to
// move that PIO's GPIO base to 16 -- machinery that only exists when
// PICO_PIO_USE_GPIO_BASE is set. It is already the default on the Fruit Jam
// (hardware/pio.h derives it from NUM_BANK0_GPIOS > 32, and the RP2350B has
// 48), so nothing sets it by hand; this only makes sure we find out at build
// time if that ever changes. Without it the claim still succeeds and then
// quietly drives pin 0 instead of pin 32.
#if DOOM_VU_WS2812_PIN >= 32 && !PICO_PIO_USE_GPIO_BASE
#error "DOOM_VU_WS2812_PIN >= 32 requires PICO_PIO_USE_GPIO_BASE (RP2350B only)"
#endif

#define VU_LED_COUNT 5
#define VU_LED_HZ    800000

// Upstream's colours (vumeter.cpp), already in the WS2812's GRB word order:
// g << 16 | r << 8 | b. Index 0 is green, index 4 is red.
static const uint32_t vu_colors[VU_LED_COUNT] = {
    0x00FF0000u, // (r,g,b) = (0,255,0)
    0x00BF4000u, // (64,191,0)
    0x00808000u, // (128,128,0)
    0x0040BF00u, // (191,64,0)
    0x0000FF00u, // (255,0,0)
};

// Written once by doom_leds_init(), and only after the state machine is fully
// configured -- everything else treats a NULL here as "no strip, do nothing".
// pico_shared's version has no such guard and faults on a NULL pio if the
// claim ever fails.
static PIO vu_pio;
static uint vu_sm;

// The whole cross-core interface: peak of the per-chunk mean |left| since the
// last repaint. Written by the mixer on either core, read and cleared by the
// frame hook on core0.
static volatile uint32_t vu_level;

// Core0-only AGC state. vu_dynamic_max starts at upstream's guess.
static float vu_dynamic_max = 2000.0f;
static uint32_t vu_last_us;

static void vu_put(uint32_t grb)
{
    // The TX FIFO is joined (8 deep, see ws2812.pio) and is always empty when
    // we get here: the previous frame's 5 words shifted out in 150 us, at
    // least a frame ago. So these never actually block.
    pio_sm_put_blocking(vu_pio, vu_sm, grb << 8u);
}

static void vu_blank(void)
{
    for (int i = 0; i < VU_LED_COUNT; i++) {
        vu_put(0);
    }
}

void doom_vu_add_chunk(const int16_t *stereo, unsigned frames)
{
    if (!frames) return;

    // Left channel only, like upstream. 256 frames costs about 3.4 us at
    // 378 MHz, once per 5.3 ms of audio.
    uint32_t sum = 0;
    for (unsigned i = 0; i < frames; i++) {
        int v = stereo[i * 2];
        sum += (uint32_t)(v < 0 ? -v : v);
    }
    uint32_t avg = sum / frames; // <= 32768, and sum cannot overflow: the
                                 // mixer's chunk is 256 frames.

    if (avg > vu_level) {
        vu_level = avg;
    }
}

static void vu_frame(void)
{
    if (!vu_pio) return;

    uint32_t avg = vu_level;
    vu_level = 0;

    uint32_t now = time_us_32();
    uint32_t dt = now - vu_last_us; // unsigned, so the 32-bit wrap is fine
    vu_last_us = now;
    // A save, a screen wipe or a WHD load can stall the frame loop; without a
    // clamp one long gap would decay the AGC to the floor in a single step.
    if (dt > 100000u) dt = 100000u;

    // Upstream snaps up to new peaks and decays by 0.995 per update. Its
    // updates are a fixed 21.77 ms apart (960 samples at 44.1 kHz), which
    // makes that a time constant of -0.021768 / ln(0.995) = 4.3429 s. Here the
    // repaint rate is the frame rate, which varies, so decaying by a fixed
    // factor per frame would make the meter behave differently at 35 fps than
    // at 60. Scaling by dt instead reproduces the same wall-clock time
    // constant at any rate; over one frame dt/tau is ~0.007, small enough that
    // the linearisation is indistinguishable from exp(-dt/tau).
    if ((float)avg > vu_dynamic_max) {
        vu_dynamic_max = (float)avg;
    } else {
        vu_dynamic_max -= vu_dynamic_max * ((float)dt * (1.0f / 4342900.0f));
    }
    // Not upstream's: its dynamic_max decays without a floor, and at zero the
    // divide below is 0/0. NaN fails the > 1.0f test, so the clamp would not
    // catch it and the cast to int would be undefined.
    if (vu_dynamic_max < 500.0f) vu_dynamic_max = 500.0f;

    float norm = (float)avg / vu_dynamic_max;
    if (norm > 1.0f) norm = 1.0f;
    int lit = (int)(norm * VU_LED_COUNT);
    if (lit > VU_LED_COUNT) lit = VU_LED_COUNT;

    // Pushed high index first, as upstream does, so the red end of the ramp
    // lands on the pixel nearest the controller.
    for (int i = VU_LED_COUNT - 1; i >= 0; i--) {
        vu_put(i < lit ? vu_colors[i] : 0);
    }
}

static void vu_init(void)
{
    PIO pio;
    uint sm, offset;

    // Ask for whatever is free rather than naming a PIO. On the Fruit Jam
    // pio0 is Pico-PIO-USB and pio1 is I2S, both with claimed state machines
    // and a GPIO base of 0; since pin 32 needs a base of 16 and the base can
    // only be changed on a completely unused PIO, the SDK can only land on
    // pio2. It cannot take a state machine from audio or USB whatever the
    // init order. Note the side effect: pio2's GPIO base becomes 16, so any
    // future pio2 user on this board is confined to GPIO 16-47.
    if (!pio_claim_free_sm_and_add_program_for_gpio_range(
            &ws2812_program, &pio, &sm, &offset, DOOM_VU_WS2812_PIN, 1, true)) {
        printf("VU meter: no free PIO/SM for GPIO %d, disabled\n", DOOM_VU_WS2812_PIN);
        return; // vu_pio stays NULL
    }
    ws2812_program_init(pio, sm, offset, DOOM_VU_WS2812_PIN, VU_LED_HZ, false);

    vu_sm = sm;
    __compiler_memory_barrier(); // core1 is already mixing by now; publish last
    vu_pio = pio;

    // gpio_base must read 16 here. ws2812_program_init throws away
    // pio_sm_init's return value, so a bad base would otherwise be silent.
    printf("VU meter: GPIO %d on pio%u sm%u offset %u, gpio_base %u, clk_sys %lu Hz\n",
           DOOM_VU_WS2812_PIN, (unsigned)PIO_NUM(pio), sm, offset,
           pio_get_gpio_base(pio), (unsigned long)clock_get_hz(clk_sys));

    vu_last_us = time_us_32();
    vu_blank();
}
#endif // DOOM_VU_WS2812_PIN >= 0

void doom_leds_init(void)
{
#if DOOM_LED_PIN >= 0
    gpio_init(DOOM_LED_PIN);
    gpio_set_dir(DOOM_LED_PIN, GPIO_OUT);
    gpio_put(DOOM_LED_PIN, 1);
#endif
#if DOOM_VU_WS2812_PIN >= 0
    vu_init();
#endif
}

void doom_leds_frame(unsigned frame)
{
#if DOOM_LED_PIN >= 0
    gpio_put(DOOM_LED_PIN, ((frame / 60u) & 1u) != 0);
#endif
#if DOOM_VU_WS2812_PIN >= 0
    vu_frame();
#endif
#if DOOM_LED_PIN < 0 && DOOM_VU_WS2812_PIN < 0
    (void)frame;
#endif
}

void doom_leds_off(void)
{
#if DOOM_LED_PIN >= 0
    gpio_put(DOOM_LED_PIN, 0);
#endif
#if DOOM_VU_WS2812_PIN >= 0
    if (vu_pio) vu_blank();
#endif
}

#endif // PICO_ON_DEVICE
