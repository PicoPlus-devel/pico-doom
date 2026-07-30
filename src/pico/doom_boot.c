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

#include "pico.h"

#include <stdio.h>
#include <stdlib.h>

#if PICO_ON_DEVICE
#include "pico/stdio.h"
#include "hardware/watchdog.h"
#endif

#include "doom_boot.h"

// pico_emuLoader bootloader handshake. Two watchdog scratch registers carry
// one-direction signals between the resident pico-bootLoader and the app it
// launched. Both survive watchdog_reboot() with pc == 0 (which only clobbers
// scratch[4]) and are cleared by a cold reset, so the "launched from
// bootloader" semantic resets correctly on power-cycle or a BOOTSEL flash.
//
//   scratch[6]: bootloader -> app. Set by the loader right before it jumps to
//               our reset vector. Read by doom_launched_from_bootloader().
//   scratch[7]: app -> bootloader. Set here right before the reset. The loader
//               checks and clears it in its resume path; if present it skips
//               the resume jump and shows the picker instead.
//
// This is a local copy of the contract in pico_shared/FrensHelpers.cpp (see
// ../pico-infonesPlus/pico_shared), duplicated because pico-doom vendors only
// 3rdparty/pico_shared_drivers and not the whole pico_shared library. The
// magics and scratch indices MUST stay identical to that file, or the
// bootloader's consumeReturnToBootloaderRequest() will not recognise us.
#define LOADER_LAUNCH_MAGIC   0xB007ED01u
#define LOADER_RETURN_MAGIC   0xB007BACEu
#define LOADER_LAUNCH_SCRATCH 6
#define LOADER_RETURN_SCRATCH 7

boolean doom_launched_from_bootloader(void)
{
#if PICO_ON_DEVICE
    return watchdog_hw->scratch[LOADER_LAUNCH_SCRATCH] == LOADER_LAUNCH_MAGIC;
#else
    return false;
#endif
}

void __attribute__((noreturn)) doom_reboot_to_loader(void)
{
#if PICO_ON_DEVICE
    // The bootloader owns the start of flash, so the bootrom always runs it
    // after a reset: a BUILD_FOR_BOOTLOADER image lands back in the picker and
    // a standalone image simply restarts Doom. Asking for the picker
    // explicitly is still worth it — it documents the intent, and it keeps
    // working if a future build ever uses watchdog_enable() to self-reboot
    // (which is exactly what the loader's resume path looks for).
    printf("\ndoom: quit -> reset%s\n",
           doom_launched_from_bootloader() ? " (returning to bootloader)" : "");
    stdio_flush();
    watchdog_hw->scratch[LOADER_RETURN_SCRATCH] = LOADER_RETURN_MAGIC;
    watchdog_reboot(0, 0, 1);
    for (;;) {
        tight_loop_contents();
    }
#else
    exit(0);
#endif
}

void __attribute__((noreturn)) doom_restart_game(void)
{
#if PICO_ON_DEVICE
    // Deliberately does NOT set the return-to-loader magic: this is the exit
    // screen's "DOOM" command, i.e. run the game again. A standalone image owns
    // the start of flash, so a plain reset restarts Doom. (The exit screen is
    // never reached under the bootloader — I_Quit reboots straight to the
    // picker there — so there is no case where this should reach the loader.)
    printf("\ndoom: restart\n");
    stdio_flush();
    watchdog_reboot(0, 0, 1);
    for (;;) {
        tight_loop_contents();
    }
#else
    exit(0);
#endif
}
