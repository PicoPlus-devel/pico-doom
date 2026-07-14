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
//	doom_tiny_full boot loader: copy the WHD from SD card into PSRAM.
//

#ifndef WHD_SDLOAD_H
#define WHD_SDLOAD_H

#ifdef __cplusplus
extern "C" {
#endif

// Bring up the QMI CS1 PSRAM, mount the SD card and copy WHD_SD_PATH
// (default /roms/doom/doom.whd) to the PSRAM XIP window at TINY_WAD_ADDR.
// Panics with a UART message on any failure (no PSRAM, no card, no file,
// wrong magic) — without the WHD there is no game to fall back to.
// Must run after the final clk_sys/clk_peri setup (PSRAM timing and SPI
// baudrate are derived from them) and before D_DoomMain().
void whd_sdload(void);

#ifdef __cplusplus
}
#endif

#endif // WHD_SDLOAD_H
