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
//	Wii-extension controllers over I2C (NES Classic Mini, SNES Classic Mini,
//	Wii Classic Controller (Pro)), on top of the vendored pico_shared wiipad
//	driver (3rdparty/pico_shared_drivers/wiipad). Adds the three things the
//	bare driver leaves to its caller: an early init that is safe on a bus
//	shared with the TLV320 codec, rate limiting, and hot-plug retries.
//	Ported from pico-infonesPlus (initVintageControllers in
//	pico_shared/FrensHelpers.cpp and the retry in pico_shared/menu.cpp).
//
//	This header is deliberately C-callable and does NOT include wiipad.h,
//	which is a C++ header: the call sites are d_main.c and doom_boot.c.
//

#ifndef DOOM_WIIPAD_H
#define DOOM_WIIPAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Board pins, set in <tag>_cflags.h. -1 = the board has no Wii extension port,
// same convention as NES_PIN_CLK (see i_usbhid.cpp) and DOOM_LED_PIN.
#ifndef WII_PIN_SDA
#define WII_PIN_SDA -1
#endif
#ifndef WII_PIN_SCL
#define WII_PIN_SCL -1
#endif

#if PICO_ON_DEVICE && WII_PIN_SDA >= 0 && WII_PIN_SCL >= 0

// Bring up the pad. Call once, EARLY - before I_InitSound(), because on the
// TLV320 boards the codec sits on the same SDA/SCL as the pad and an
// uninitialized SNES-Classic pad wedges the bus for it. See the call site in
// d_main.c's D_DoomMain().
void doom_wiipad_init(void);

// Latest button state, in the driver's own layout: bit0=A, 1=B, 2=Select,
// 3=Start, 4=Up, 5=Down, 6=Left, 7=Right, 8=X, 9=Y, 10=L, 11=R. Rate-limited
// internally, so it is safe to call as fast as the caller likes; also retries
// the connect handshake so a pad plugged in after boot still works.
uint16_t doom_wiipad_read(void);

// Release the bus and the pins. For the reboot paths in doom_boot.c: leaving
// the pad initialized across a watchdog reset has been seen to hang the next
// boot (pico-infonesPlus does the same before its own reboot).
void doom_wiipad_shutdown(void);

#else

#define doom_wiipad_init() ((void)0)
#define doom_wiipad_read() ((uint16_t)0)
#define doom_wiipad_shutdown() ((void)0)

#endif

#ifdef __cplusplus
}
#endif

#endif // DOOM_WIIPAD_H
