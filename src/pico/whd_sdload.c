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
//	doom_tiny_full boot loader: copy the full-game WHD (whd_gen
//	-no-super-tiny output) from the SD card into PSRAM, where the rest
//	of the engine reads it zero-copy through the cached QMI CS1 XIP
//	window (TINY_WAD_ADDR = 0x11000000, see the <board>_cflags.h
//	headers). Mount/config sequence mirrors the proven
//	Frens::initSDCard() in pico-infonesPlus/pico_shared/FrensHelpers.cpp.
//
//	Every phase logs to serial BEFORE it runs, so a hang on a headless board
//	is attributable from the UART output alone. Failures additionally put an
//	explanation on the TV via doom_error_screen(), which is the only channel
//	that works on a board built with DOOM_NO_STDIO_UART.
//

#if PICO_ON_DEVICE && WHD_LOAD_FROM_SD

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"

#include "tf_card.h"
#include "ff.h"
#include "SetupPsram.h"

#include "doom_errscreen.h"
#include "whd_sdload.h"

#ifndef SDCARD_SPI
#error WHD_LOAD_FROM_SD builds need SDCARD_SPI/SDCARD_PIO/PSRAM_CS_PIN from the board cflags header
#endif

#if TINY_WAD_ADDR != 0x11000000
#error WHD_LOAD_FROM_SD expects TINY_WAD_ADDR to be the PSRAM XIP CS1 window (0x11000000)
#endif

#ifndef WHD_SD_PATH
#define WHD_SD_PATH "/roms/doom/doom.whd"
#endif

// Read granularity: f_read copies straight into the PSRAM window, so this
// only bounds how often the progress percentage updates — no RAM buffer
// involved.
#define WHD_SDLOAD_CHUNK (256 * 1024)

static FATFS whd_sdload_fs;

static const char *fs_type_name(BYTE fs_type)
{
    switch (fs_type) {
        case FS_FAT12: return "FAT12";
        case FS_FAT16: return "FAT16";
        case FS_FAT32: return "FAT32";
        case FS_EXFAT: return "exFAT";
        default:       return "unknown";
    }
}

void whd_sdload(void)
{
    printf("\n=== doom_tiny_full: WHD from SD card ===\n");

    // The flash save-game slots pack downward from the end of flash with
    // SAVE_FLASH_BASE as their floor (p_saveg.c); the program image must
    // stay below that floor or saving would corrupt code.
    extern uint8_t __flash_binary_end;
    printf("flash: image ends at %p, save floor at %p\n",
           &__flash_binary_end, (void *)SAVE_FLASH_BASE);
    if ((uintptr_t)&__flash_binary_end > SAVE_FLASH_BASE) {
        doom_error_screen("BAD FLASH LAYOUT",
                          "The program image ends at %p, above the save-game floor"
                          " at %p.\n"
                          "\n"
                          "Saving would overwrite code. Raise SAVE_FLASH_BASE in the"
                          " board's cflags header and rebuild.",
                          &__flash_binary_end, (void *)SAVE_FLASH_BASE);
    }

    // --- PSRAM --------------------------------------------------------------
    printf("psram: init QMI CS1 on GPIO %d (clk_sys %u kHz)...\n",
           PSRAM_CS_PIN, (unsigned)(clock_get_hz(clk_sys) / 1000));
    int32_t psram_size = SetupPsram(PSRAM_CS_PIN);
    if (psram_size <= 0) {
        doom_error_screen("NO PSRAM",
                          "No PSRAM was found on GPIO %d.\n"
                          "\n"
                          "The full-game build keeps all of DOOM's data in PSRAM, so"
                          " it cannot run on a board without it. Use the doom_tiny"
                          " build instead.",
                          PSRAM_CS_PIN);
    }
    printf("psram: OK, %u KB mapped at %p\n",
           (unsigned)(psram_size / 1024), (void *)TINY_WAD_ADDR);

    // --- SD card ------------------------------------------------------------
    printf("sd: config SPI%d SCK=%d MOSI=%d MISO=%d CS=%d...\n",
           spi_get_index(SDCARD_SPI), SD_SCK, SD_TX, SD_RX, SD_CS);
    static pico_fatfs_spi_config_t config = {
        SDCARD_SPI,
        CLK_SLOW_DEFAULT,
        CLK_FAST_DEFAULT_PIO,
        SD_RX,  // MISO
        SD_CS,
        SD_SCK,
        SD_TX,  // MOSI
        true    // internal pullups on MISO/MOSI
    };
    if (pico_fatfs_set_config(&config)) {
        printf("sd: using hardware SPI (slow %u kHz, fast %u kHz)\n",
               (unsigned)(pico_fatfs_get_clk_slow_freq() / 1000),
               (unsigned)(pico_fatfs_get_clk_fast_freq() / 1000));
    } else {
        // Pins outside the hardware SPI mux tables — drive them with PIO.
        pico_fatfs_config_spi_pio(SDCARD_PIO, pio_claim_unused_sm(SDCARD_PIO, true));
        printf("sd: pins not hardware-SPI capable, using PIO SPI fallback\n");
    }

    printf("sd: mounting...\n");
    FRESULT fr = f_mount(&whd_sdload_fs, "", 1);
    if (fr != FR_OK) {
        doom_error_screen("NO SD CARD",
                          "The SD card could not be mounted (FatFs error %d).\n"
                          "\n"
                          "Insert a FAT-formatted card holding " WHD_SD_PATH
                          " and reset the board.",
                          fr);
    }
    printf("sd: mounted, filesystem %s\n", fs_type_name(whd_sdload_fs.fs_type));

    // --- WHD → PSRAM ---------------------------------------------------------
    printf("whd: opening " WHD_SD_PATH "...\n");
    FIL fil;
    fr = f_open(&fil, WHD_SD_PATH, FA_READ);
    if (fr != FR_OK) {
        doom_error_screen("GAME DATA NOT FOUND",
                          WHD_SD_PATH " could not be opened (FatFs error %d).\n"
                          "\n"
                          "Convert your WAD with:\n"
                          "\n"
                          "    whd_gen doom.wad doom.whd -no-super-tiny\n"
                          "\n"
                          "then copy doom.whd to /roms/doom on the SD card."
                          " Download whd_gen from\n"
                          "\n"
                          "    " DOOM_RELEASES_URL,
                          fr);
    }
    FSIZE_t size = f_size(&fil);
    printf("whd: %u KB, PSRAM capacity %u KB\n",
           (unsigned)(size / 1024), (unsigned)(psram_size / 1024));
    if (size < 16 || size > (FSIZE_t)psram_size) {
        doom_error_screen("WRONG GAME DATA SIZE",
                          WHD_SD_PATH " is %u bytes, but the PSRAM holds"
                          " %u KB.\n"
                          "\n"
                          "The file is truncated or was built for a different"
                          " target. Regenerate it with:\n"
                          "\n"
                          "    whd_gen doom.wad doom.whd -no-super-tiny\n"
                          "\n"
                          "Download whd_gen from\n"
                          "\n"
                          "    " DOOM_RELEASES_URL,
                          (unsigned)size, (unsigned)(psram_size / 1024));
    }

    printf("whd: loading into PSRAM: ");
    uint32_t t0 = to_ms_since_boot(get_absolute_time());
    uint8_t *dest = (uint8_t *)TINY_WAD_ADDR;
    FSIZE_t remaining = size;
    unsigned last_decade = 0;
    while (remaining) {
        UINT want = remaining < WHD_SDLOAD_CHUNK ? (UINT)remaining : WHD_SDLOAD_CHUNK;
        UINT br = 0;
        fr = f_read(&fil, dest, want, &br);
        if (fr != FR_OK || br == 0) {
            doom_error_screen("SD CARD READ ERROR",
                              "Reading " WHD_SD_PATH " failed at offset %u"
                              " (FatFs error %d).\n"
                              "\n"
                              "Reseat the card, or copy doom.whd to it again.",
                              (unsigned)(size - remaining), fr);
        }
        dest += br;
        remaining -= br;
        // one "10%.. 20%.." tick per decade so the line stays short
        unsigned decade = (unsigned)(((size - remaining) * 10) / size);
        if (decade > last_decade) {
            last_decade = decade;
            printf("%u%%%s", decade * 10, decade == 10 ? "\n" : ".. ");
        }
    }
    f_close(&fil);
    uint32_t ms = to_ms_since_boot(get_absolute_time()) - t0;
    if (ms == 0) ms = 1;
    printf("whd: loaded %u KB in %u ms (%u KB/s)\n",
           (unsigned)(size / 1024), (unsigned)ms,
           (unsigned)((size * 1000) / ms / 1024));

    // --- validate -------------------------------------------------------------
    const uint8_t *whd = (const uint8_t *)TINY_WAD_ADDR;
    if (whd[0] != 'I' || whd[1] != 'W' || whd[2] != 'H' || whd[3] != 'D') {
        doom_error_screen("BAD GAME DATA",
                          WHD_SD_PATH " is not a WHD file (magic"
                          " %02x%02x%02x%02x, expected 49574844).\n"
                          "\n"
                          "Generate it with:\n"
                          "\n"
                          "    whd_gen doom.wad doom.whd -no-super-tiny\n"
                          "\n"
                          "Download whd_gen from\n"
                          "\n"
                          "    " DOOM_RELEASES_URL,
                          whd[0], whd[1], whd[2], whd[3]);
    }
    // Peek the whdheader_t that sits right after the 12-byte wadinfo_t (see
    // whddata.h / w_wad.c) so the log identifies exactly which WAD this is:
    //   +12 uint32 size, +16 uint32 hash, +20 char name[14]
    uint32_t whd_size, whd_hash;
    char whd_name[15];
    memcpy(&whd_size, whd + 12, 4);
    memcpy(&whd_hash, whd + 16, 4);
    memcpy(whd_name, whd + 20, 14);
    whd_name[14] = 0;
    printf("whd: magic OK, name \"%s\", hash %08x, internal size %u KB\n",
           whd_name, (unsigned)whd_hash, (unsigned)(whd_size / 1024));
    if (whd_size != (uint32_t)size) {
        printf("whd: WARNING header size %u != file size %u\n",
               (unsigned)whd_size, (unsigned)size);
    }
    printf("=== WHD ready, starting Doom ===\n");
}

#endif // PICO_ON_DEVICE && WHD_LOAD_FROM_SD
