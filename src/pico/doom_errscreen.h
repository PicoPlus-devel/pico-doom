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
//	Fatal-error screen: say what is wrong on the TV instead of dying quietly.
//

#ifndef DOOM_ERRSCREEN_H
#define DOOM_ERRSCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

// Where the prebuilt firmware, doom1-whx.uf2 and the whd_gen host tool are
// attached. Quoted on the error pages, so keep it inside the 72-column body.
#define DOOM_RELEASES_URL "https://github.com/fhoedemakers/pico-doom/releases"

// Show a full-screen DOS-style error page and never return.
//
// `heading` is one short line for the red banner (upper case reads best);
// `fmt`/... is printf-style body text where '\n' starts a new line and long
// lines word-wrap. The same text goes to stdout first, so a board with UART
// enabled still gets it in the boot log.
//
// Usable from as early as whd_sdload() — before I_Init(), Z_Init() and any WAD:
// the 80x25 text mode it borrows from the exit screen renders a font that is
// compiled into the binary, out of buffers that live in .bss. It brings the
// video pipeline up itself if I_InitGraphics() has not run yet, and falls back
// to panic() on the (unshipped) scanvideo builds that cannot do that.
void __attribute__((noreturn)) doom_error_screen(const char *heading,
                                                 const char *fmt, ...)
        __attribute__((format(printf, 2, 3)));

#ifdef __cplusplus
}
#endif

#endif // DOOM_ERRSCREEN_H
