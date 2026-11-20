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
//	pico-bootLoader handshake: leave the game the way we came in.
//

#ifndef DOOM_BOOT_H
#define DOOM_BOOT_H

// Plain <stdbool.h> rather than doomtype.h so this stays safe to include from
// the C++ translation units (i_usbhid.cpp).
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// True if this image was started by the resident pico-bootLoader rather than
// flashed standalone via BOOTSEL. Informational only — the reboot below does
// the right thing either way.
bool doom_launched_from_bootloader(void);

#if defined(BUILD_FOR_BOOTLOADER) && BUILD_FOR_BOOTLOADER
// pico-bootLoader attract mode (doom_boot.c): the loader started Doom to show
// it off. The demo loop runs; a key, button or mouse button hands Doom to the
// player (doom_attract_input, from D_PostEvent), and when nobody has done so
// within the game time doom_attract_tick (from D_RunFrame) returns to the loader,
// which starts its next application.
bool doom_attract_active(void);
void doom_attract_input(void);
void doom_attract_tick(void);
// Back to the loader for its next application, attract word kept. Never returns.
void __attribute__((noreturn)) doom_attract_hand_back(void);
#endif

// End the program: return to the pico-bootLoader picker if we were launched
// from it, otherwise just reset (which restarts Doom). Never returns.
void __attribute__((noreturn)) doom_reboot_to_loader(void);

// Run the game again — the exit screen's "DOOM" command. Resets without asking
// the bootloader for its picker. Never returns.
void __attribute__((noreturn)) doom_restart_game(void);

// Reset into the chip's ROM bootloader, so the board comes up as the RP2350
// UF2 drive and can be reflashed without holding BOOTSEL while replugging.
// This is the Options menu's "Bootsel Mode" item. Note this is a different
// destination from doom_reboot_to_loader() above, which returns to the
// resident pico-bootLoader picker. Flushes settings and blanks the LEDs on the
// way out, because nothing else on this path does. Never returns.
void __attribute__((noreturn)) doom_reboot_to_bootsel(void);

// The display mode of the HDMI connector, true for DVI. The mode to start in:
// the bootloader's DISPLAY= from /boot.txt when it started us, else the stored
// setting, else the board default (DVI on the Murmulator M2).
bool doom_display_boot_dvi(void);

// The running mode, and the Options menu's "Display" item: switches the
// picture and the sound, and stores the choice in /settings_DOOM.dat.
bool doom_display_is_dvi(void);
void doom_display_toggle(void);

#ifdef __cplusplus
}
#endif

#endif // DOOM_BOOT_H
