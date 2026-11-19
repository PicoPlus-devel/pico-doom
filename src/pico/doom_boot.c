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
#include "pico/bootrom.h"
#include "hardware/watchdog.h"
#include "pico/time.h"
#include "doom_wiipad.h"
#endif

#include "doom_boot.h"
// Both stub themselves out off-device, so doom_reboot_to_bootsel() can call
// them without another #if -- the same arrangement I_Quit() uses.
#include "doom_settings.h"
#include "doom_sdcard.h"
#if PICO_ON_DEVICE
#include "pico_bootcfg.h"
#include "pico_hdmi_glue.h"
#include "video_output.h"
#endif
#include "doom_leds.h"

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

bool doom_launched_from_bootloader(void)
{
#if PICO_ON_DEVICE
    return watchdog_hw->scratch[LOADER_LAUNCH_SCRATCH] == LOADER_LAUNCH_MAGIC;
#else
    return false;
#endif
}

#if BUILD_FOR_BOOTLOADER
// pico-bootLoader attract mode. Local copy of the contract in
// pico_shared/attract.h (same reason as above): when the loader starts Doom for
// its attract mode, scratch[7] holds LOADER_ATTRACT_MAGIC with the game time in
// minutes in the low byte. The first input clears it and Doom is the player's;
// otherwise, once the time is up, Doom resets with scratch[7] left as it is and
// the loader goes on to its next application.
#define LOADER_ATTRACT_MAGIC      0xB007A000u
#define LOADER_ATTRACT_MAGIC_MASK 0xFFFFF000u
#define LOADER_ATTRACT_MINUTES    0x000000FFu

#if PICO_ON_DEVICE
#include "pico/binary_info.h"
// The loader picks Doom for attract mode only when this is in the image and the
// game data is on the card. doom_tiny's WAD is a companion .uf2 the loader
// checks itself; doom_tiny_full reads the WHD from the card into PSRAM, so it
// also asks for a board with PSRAM ("@psram").
#if WHD_LOAD_FROM_SD
bi_decl(bi_program_feature("attract:@psram|/roms/doom/doom.whd"));
#else
bi_decl(bi_program_feature("attract:"));
#endif
#endif

bool doom_attract_active(void)
{
#if PICO_ON_DEVICE
    const uint32_t w = watchdog_hw->scratch[LOADER_RETURN_SCRATCH];
    return doom_launched_from_bootloader() &&
           (w & LOADER_ATTRACT_MAGIC_MASK) == LOADER_ATTRACT_MAGIC &&
           (w & LOADER_ATTRACT_MINUTES) != 0;
#else
    return false;
#endif
}

void doom_attract_input(void)
{
#if PICO_ON_DEVICE
    if (!doom_attract_active())
        return;
    watchdog_hw->scratch[LOADER_RETURN_SCRATCH] = 0;
    printf("doom: attract mode: a player took over\n");
#endif
}

void doom_attract_tick(void)
{
#if PICO_ON_DEVICE
    if (!doom_attract_active())
        return;
    const uint32_t minutes = watchdog_hw->scratch[LOADER_RETURN_SCRATCH] & LOADER_ATTRACT_MINUTES;
    if (time_us_64() < (uint64_t)minutes * 60u * 1000000u)
        return;
    printf("\ndoom: attract mode: time is up, back to the bootloader\n");
    doom_attract_hand_back();
#endif
}

void doom_attract_hand_back(void)
{
#if PICO_ON_DEVICE
    stdio_flush();
    // As doom_reboot_to_loader(), but scratch[7] keeps the attract word, so the
    // loader goes on to its next application.
    doom_wiipad_shutdown();
    watchdog_reboot(0, 0, 1);
#endif
    for (;;) {
        tight_loop_contents();
    }
}
#endif

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
    // Hand the I2C bus back before the reset. pico-infonesPlus does the same
    // before its own reboot: a Wii pad left initialized across the reset has
    // been seen to hang the next boot.
    doom_wiipad_shutdown();
    watchdog_hw->scratch[LOADER_RETURN_SCRATCH] = LOADER_RETURN_MAGIC;
    watchdog_reboot(0, 0, 1);
    for (;;) {
        tight_loop_contents();
    }
#else
    exit(0);
#endif
}

void __attribute__((noreturn)) doom_reboot_to_bootsel(void)
{
#if PICO_ON_DEVICE
    printf("\ndoom: bootsel\n");
    stdio_flush();
    // Unlike the two resets above, this one is entered straight from the
    // Options menu: M_ClearMenus() never runs, so its flush never happens, and
    // I_AtExit handlers never run here either. Without this the player loses
    // whatever they changed in the very menu they are standing in.
    doom_settings_flush();
    // The reset clears the PIO but not the pixels' latched colours, so the VU
    // meter would otherwise stay lit through the whole BOOTSEL session.
    doom_leds_off();
    doom_wiipad_shutdown(); // see doom_reboot_to_loader()
    // Into the ROM bootloader, i.e. the RP2350 UF2 drive -- not the resident
    // pico-bootLoader picker that doom_reboot_to_loader() goes back to. Both
    // arguments 0: no activity-LED GPIO, and leave both USB MSD and PICOBOOT
    // enabled. pico_bootrom comes in transitively via hardware_watchdog, so
    // this needs no CMake change.
    reset_usb_boot(0, 0);
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
    doom_wiipad_shutdown(); // see doom_reboot_to_loader()
    watchdog_reboot(0, 0, 1);
    for (;;) {
        tight_loop_contents();
    }
#else
    exit(0);
#endif
}

// The display mode (HDMI or DVI) of the HDMI connector.
//
// At start: the bootloader's DISPLAY= setting from /boot.txt when it started
// us, else the stored setting (the Options menu's "Display", in
// /settings_DOOM.dat), else the board default (DVI on the Murmulator M2, HDMI
// elsewhere; see pico_bootcfg.h).
bool doom_display_boot_dvi(void)
{
#if PICO_ON_DEVICE
    return doom_sd_boot_dvi(doom_settings_display_pref());
#else
    return false;
#endif
}

bool doom_display_is_dvi(void)
{
#if PICO_ON_DEVICE
    return video_output_get_dvi_mode();
#else
    return false;
#endif
}

// The Options menu's "Display" item: switches the running picture and the
// sound with it, and stores the choice (M_ChangeDisplay writes it at once).
void doom_display_toggle(void)
{
#if PICO_ON_DEVICE
    bool dvi = !video_output_get_dvi_mode();
    doom_hdmi_set_dvi(dvi);
    doom_settings_set_display_pref(dvi ? PICO_DISPLAY_DVI : PICO_DISPLAY_HDMI);
#endif
}
