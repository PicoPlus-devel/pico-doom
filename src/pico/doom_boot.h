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

// End the program: return to the pico-bootLoader picker if we were launched
// from it, otherwise just reset (which restarts Doom). Never returns.
void __attribute__((noreturn)) doom_reboot_to_loader(void);

// Run the game again — the exit screen's "DOOM" command. Resets without asking
// the bootloader for its picker. Never returns.
void __attribute__((noreturn)) doom_restart_game(void);

#ifdef __cplusplus
}
#endif

#endif // DOOM_BOOT_H
