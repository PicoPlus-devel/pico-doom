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
//	The one SD card the whole port talks to: mount plus small whole-file
//	helpers. Save games (/SAVES/doomsav<N>.dsg) and settings
//	(/settings_DOOM.dat) live here; doom_tiny_full additionally streams the
//	WHD off it at boot (whd_sdload.c).
//

#ifndef DOOM_SDCARD_H
#define DOOM_SDCARD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Where save games go. Matches pico-infonesPlus' GAMESAVEDIR so a card shared
// between the two keeps all of its saves in one place.
#define DOOM_SD_SAVEDIR "/SAVES"

// Configure the SPI (or PIO-SPI) pins from the board's cflags header and mount
// the card. Safe to call more than once: a second call while already mounted is
// a no-op returning true.
//
// Returns false — it never panics and never draws an error screen — when the
// pins cannot be driven, no card is present, or the card holds no filesystem.
// The caller decides how bad that is: whd_sdload() cannot continue without game
// data, while a standalone build just loses persistence for the session.
//
// Must run after the final clk_sys/clk_peri setup: the SPI baudrate derives
// from clk_peri.
bool doom_sd_mount(void);

// True when the card is usable. If the mount at boot did not take, this retries
// it, so a card inserted after power-on starts working without a reset. Every
// helper below calls this first.
bool doom_sd_ready(void);

// Read a whole file into buf. Fails (returning false) if the file is missing,
// unreadable, or larger than bufsize — *out_size, when non-NULL, receives the
// number of bytes read on success and the full file size when it did not fit,
// so callers can tell "too big" from "not there".
bool doom_sd_read_file(const char *path, void *buf, uint32_t bufsize, uint32_t *out_size);

// Read the first n bytes of a file. Short files fail rather than returning a
// partial buffer.
bool doom_sd_peek_file(const char *path, void *buf, uint32_t n);

// Create/truncate path and write size bytes. Creates the parent directory when
// it is one level deep (i.e. /SAVES). Returns the FatFs FRESULT: FR_OK (0) on
// success, FR_NOT_READY when there is no usable card.
int doom_sd_write_file(const char *path, const void *buf, uint32_t size);

// Delete path. Missing file counts as success.
bool doom_sd_delete_file(const char *path);

// File size in bytes, or -1 when the file does not exist / no card.
int32_t doom_sd_file_size(const char *path);

// Human-readable FatFs FRESULT, for logs and on-screen messages.
const char *doom_sd_strerror(int fresult);

#ifdef __cplusplus
}
#endif

#endif // DOOM_SDCARD_H
