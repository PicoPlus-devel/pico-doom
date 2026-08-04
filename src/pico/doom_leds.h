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
//	Status output on whatever LEDs the board carries: a heartbeat on the
//	plain onboard LED, and on the Fruit Jam a 5-pixel WS2812 audio VU meter.
//	Ported from pico-infonesPlus (pico_shared/vumeter.cpp and
//	Frens::blinkLed in pico_shared/FrensHelpers.cpp).
//

#ifndef DOOM_LEDS_H
#define DOOM_LEDS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Board pins, set in <tag>_cflags.h. -1 = the board does not have it, same
// convention as NES_PIN_CLK (see i_usbhid.cpp).
#ifndef DOOM_LED_PIN
#define DOOM_LED_PIN -1
#endif
#ifndef DOOM_VU_WS2812_PIN
#define DOOM_VU_WS2812_PIN -1
#endif

#if PICO_ON_DEVICE

// Claim the pins. Call once, late: after the final clk_sys (the WS2812 clock
// divider is computed from it, once) and after every other PIO consumer has
// claimed, so a failure here means "nothing was free" rather than "we took
// audio's state machine". See the call site in d_main.c's D_DoomLoop().
void doom_leds_init(void);

// Per-frame housekeeping: advance the heartbeat and repaint the VU meter.
// Core0 only — it is the only place the PIO FIFO is written.
void doom_leds_frame(unsigned frame);

// LED off, strip dark. For the quit path, where doom_leds_frame() stops being
// called but the pixels keep whatever they were last given.
void doom_leds_off(void);

#if DOOM_VU_WS2812_PIN >= 0
// Feed the VU meter one mix chunk (int16 stereo interleaved, `frames` frames).
// Called from the mixer, so it may run on EITHER core; see doom_leds.c for why
// the hand-off to doom_leds_frame() needs no lock.
void doom_vu_add_chunk(const int16_t *stereo, unsigned frames);
#else
#define doom_vu_add_chunk(stereo, frames) ((void)(stereo), (void)(frames))
#endif

#else

#define doom_leds_init() ((void)0)
#define doom_leds_frame(frame) ((void)(frame))
#define doom_leds_off() ((void)0)
#define doom_vu_add_chunk(stereo, frames) ((void)(stereo), (void)(frames))

#endif

#ifdef __cplusplus
}
#endif

#endif // DOOM_LEDS_H
