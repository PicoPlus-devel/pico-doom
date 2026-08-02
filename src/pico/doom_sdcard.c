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
//	The one SD card the whole port talks to. Owns the FATFS, the pin
//	configuration and a single FIL, and hands out whole-file read/write
//	helpers on top. Nothing here ever panics or calls I_Error: a missing or
//	yanked card costs you persistence, not the session.
//
//	Mount/config sequence mirrors the proven Frens::initSDCard() in
//	pico-infonesPlus/pico_shared/FrensHelpers.cpp, and every phase logs to
//	serial before it runs so a hang on a headless board is attributable from
//	the UART output alone.
//

#if PICO_ON_DEVICE

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"

#include "tf_card.h"
#include "ff.h"

#include "doom_sdcard.h"

#if !defined(SDCARD_SPI) || !defined(SDCARD_PIO)
#error the board cflags header must define SDCARD_SPI and SDCARD_PIO
#endif
#if !defined(SD_TX) || !defined(SD_RX) || !defined(SD_SCK) || !defined(SD_CS)
#error the board cflags header must define SD_TX/SD_RX/SD_SCK/SD_CS
#endif

// How long to wait before probing again after a failed mount. Without this a
// cardless board would re-probe the bus once per save slot every time the Load
// menu opens, which is both slow and pointless.
#define SD_RETRY_MS 3000

static FATFS sd_fs;
// One file at a time: every helper below opens, uses and closes before
// returning, so a single shared FIL saves ~600 bytes of zone heap over giving
// each call site its own. Note it is far too big for the 4 KB core-0 stack.
static FIL sd_fil;

static bool sd_mounted;
static bool sd_pins_configured;
static uint32_t sd_next_retry_ms;

const char *doom_sd_strerror(int fresult)
{
    switch ((FRESULT)fresult) {
        case FR_OK:               return "ok";
        case FR_DISK_ERR:         return "disk error";
        case FR_NOT_READY:        return "drive not ready";
        case FR_NO_FILE:          return "no such file";
        case FR_NO_PATH:          return "no such directory";
        case FR_DENIED:           return "denied or disk full";
        case FR_EXIST:            return "already exists";
        case FR_WRITE_PROTECTED:  return "card is write protected";
        case FR_NO_FILESYSTEM:    return "no FAT filesystem";
        case FR_TIMEOUT:          return "timeout";
        default:                  return "error";
    }
}

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

// Distinguish "the card is fine, this request was not" from "the card went
// away". Only the latter drops the mount, so a reseated (or newly inserted)
// card is picked up by the next call instead of needing a reset.
static void sd_note_result(FRESULT fr)
{
    switch (fr) {
        case FR_OK:
        case FR_NO_FILE:
        case FR_NO_PATH:
        case FR_EXIST:
        case FR_DENIED:
        case FR_WRITE_PROTECTED:
            return;
        default:
            break;
    }
    printf("sd: dropping mount after %s (%d)\n", doom_sd_strerror(fr), fr);
    f_mount(NULL, "", 0);
    sd_mounted = false;
    sd_next_retry_ms = to_ms_since_boot(get_absolute_time()) + SD_RETRY_MS;
}

bool doom_sd_mount(void)
{
    if (sd_mounted) {
        return true;
    }

    if (!sd_pins_configured) {
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
            // All four supported boards wire the card to hardware-SPI-capable
            // pins, so this is a safety net; claim without `required` so a build
            // that has already spent its state machines on video/USB/nespad
            // loses the card rather than panicking at boot.
            int sm = pio_claim_unused_sm(SDCARD_PIO, false);
            if (sm < 0) {
                printf("sd: pins are not hardware-SPI capable and no PIO state"
                       " machine is free - no SD card support\n");
                sd_next_retry_ms = to_ms_since_boot(get_absolute_time()) + SD_RETRY_MS;
                return false;
            }
            pico_fatfs_config_spi_pio(SDCARD_PIO, (uint)sm);
            printf("sd: pins not hardware-SPI capable, using PIO SPI fallback"
                   " (sm %d)\n", sm);
        }
        sd_pins_configured = true;
    }

    printf("sd: mounting...\n");
    FRESULT fr = f_mount(&sd_fs, "", 1);
    if (fr != FR_OK) {
        printf("sd: mount failed: %s (%d)\n", doom_sd_strerror(fr), fr);
        // Arm the backoff here too, not just in doom_sd_ready(): the boot-time
        // call comes straight from i_main.c, and without this the settings load
        // a moment later would probe the empty slot all over again.
        sd_next_retry_ms = to_ms_since_boot(get_absolute_time()) + SD_RETRY_MS;
        return false;
    }
    printf("sd: mounted, filesystem %s\n", fs_type_name(sd_fs.fs_type));
    sd_mounted = true;
    return true;
}

bool doom_sd_ready(void)
{
    if (sd_mounted) {
        return true;
    }
    // Signed difference so the ~24.8 day wrap of to_ms_since_boot() is harmless.
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if ((int32_t)(now - sd_next_retry_ms) < 0) {
        return false;
    }
    sd_next_retry_ms = now + SD_RETRY_MS;
    return doom_sd_mount();
}

// Create the directory holding `path`, one level deep (i.e. /SAVES). Anything
// deeper is not needed by this port and is left to the user.
static void ensure_parent_dir(const char *path)
{
    const char *slash = strrchr(path, '/');
    if (!slash || slash == path) {
        return; // file sits in the root
    }
    char dir[32];
    size_t len = (size_t)(slash - path);
    if (len >= sizeof(dir)) {
        return;
    }
    memcpy(dir, path, len);
    dir[len] = '\0';
    FRESULT fr = f_mkdir(dir);
    if (fr != FR_OK && fr != FR_EXIST) {
        printf("sd: mkdir %s: %s (%d)\n", dir, doom_sd_strerror(fr), fr);
    }
}

bool doom_sd_read_file(const char *path, void *buf, uint32_t bufsize, uint32_t *out_size)
{
    if (out_size) {
        *out_size = 0;
    }
    if (!doom_sd_ready()) {
        return false;
    }

    FRESULT fr = f_open(&sd_fil, path, FA_READ);
    if (fr != FR_OK) {
        if (fr != FR_NO_FILE) {
            printf("sd: open %s: %s (%d)\n", path, doom_sd_strerror(fr), fr);
        }
        sd_note_result(fr);
        return false;
    }

    FSIZE_t fsize = f_size(&sd_fil);
    if (fsize > bufsize) {
        // Report the real size so the caller can say "too big" rather than
        // "missing".
        if (out_size) {
            *out_size = (uint32_t)fsize;
        }
        printf("sd: %s is %u bytes, buffer is %u\n",
               path, (unsigned)fsize, (unsigned)bufsize);
        f_close(&sd_fil);
        return false;
    }

    UINT br = 0;
    fr = f_read(&sd_fil, buf, (UINT)fsize, &br);
    FRESULT fc = f_close(&sd_fil);
    if (fr == FR_OK) {
        fr = fc;
    }
    if (fr != FR_OK || br != fsize) {
        printf("sd: read %s: %s (%d), %u of %u bytes\n",
               path, doom_sd_strerror(fr), fr, (unsigned)br, (unsigned)fsize);
        sd_note_result(fr);
        return false;
    }

    // Logged like the write side, so a save game or settings load is visible in
    // the boot/serial trace rather than being the one card access that happens
    // silently.
    printf("sd: read %u bytes from %s\n", (unsigned)br, path);
    if (out_size) {
        *out_size = br;
    }
    return true;
}

bool doom_sd_peek_file(const char *path, void *buf, uint32_t n)
{
    if (!doom_sd_ready()) {
        return false;
    }

    FRESULT fr = f_open(&sd_fil, path, FA_READ);
    if (fr != FR_OK) {
        if (fr != FR_NO_FILE) {
            printf("sd: open %s: %s (%d)\n", path, doom_sd_strerror(fr), fr);
        }
        sd_note_result(fr);
        return false;
    }

    UINT br = 0;
    fr = f_read(&sd_fil, buf, (UINT)n, &br);
    FRESULT fc = f_close(&sd_fil);
    if (fr == FR_OK) {
        fr = fc;
    }
    if (fr != FR_OK || br != n) {
        sd_note_result(fr);
        return false;
    }
    return true;
}

int doom_sd_write_file(const char *path, const void *buf, uint32_t size)
{
    if (!doom_sd_ready()) {
        return FR_NOT_READY;
    }
    ensure_parent_dir(path);

    FRESULT fr = f_open(&sd_fil, path, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        printf("sd: create %s: %s (%d)\n", path, doom_sd_strerror(fr), fr);
        sd_note_result(fr);
        return fr;
    }

    UINT bw = 0;
    fr = f_write(&sd_fil, buf, (UINT)size, &bw);
    FRESULT fc = f_close(&sd_fil);
    if (fr == FR_OK && bw != size) {
        fr = FR_DENIED; // FatFs reports a full disk as a short write
    }
    if (fr == FR_OK) {
        fr = fc;
    }
    if (fr != FR_OK) {
        printf("sd: write %s: %s (%d), %u of %u bytes\n",
               path, doom_sd_strerror(fr), fr, (unsigned)bw, (unsigned)size);
        // Don't leave a truncated file where a good one used to be.
        f_unlink(path);
        sd_note_result(fr);
        return fr;
    }

    printf("sd: wrote %u bytes to %s\n", (unsigned)bw, path);
    return FR_OK;
}

bool doom_sd_delete_file(const char *path)
{
    if (!doom_sd_ready()) {
        return false;
    }
    FRESULT fr = f_unlink(path);
    if (fr == FR_OK || fr == FR_NO_FILE) {
        return true;
    }
    printf("sd: delete %s: %s (%d)\n", path, doom_sd_strerror(fr), fr);
    sd_note_result(fr);
    return false;
}

int32_t doom_sd_file_size(const char *path)
{
    if (!doom_sd_ready()) {
        return -1;
    }
    // f_open rather than f_stat: FILINFO carries a 256-character long-name
    // buffer, which is not something to put on the core-0 stack.
    FRESULT fr = f_open(&sd_fil, path, FA_READ);
    if (fr != FR_OK) {
        sd_note_result(fr);
        return -1;
    }
    FSIZE_t size = f_size(&sd_fil);
    f_close(&sd_fil);
    return (int32_t)size;
}

#endif // PICO_ON_DEVICE
