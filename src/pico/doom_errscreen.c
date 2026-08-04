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
//	Fatal-error screen. Where the code used to panic() over missing game
//	data, put a readable page on the TV instead: panic() prints to UART and
//	spins, but murmulatorm2 builds with DOOM_NO_STDIO_UART=1 and no board
//	enables stdio-USB, so on those the user got a silent black screen.
//
//	Everything here reuses the exit screen's 80x25 VGA text mode (I_Endoom,
//	the A:\> DOS prompt) via text_screen_prepare()/text_screen_show(). That
//	mode needs no WAD and no zone allocator — its 8x16 CP437 font is
//	compiled into the binary and its buffers are carved out of the
//	renderer's idle .bss work area — so it also works at the very first
//	failure point, whd_sdload(), long before Z_Init().
//

#if PICO_ON_DEVICE

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "doomtype.h"
#include "i_video.h"

#include "doom_errscreen.h"

// VGA attribute byte: high nibble background, low nibble foreground, indexing
// the ega_colors[] table in i_video.c.
#define ATTR_BANNER 0x4f    // bright white on red
#define ATTR_BODY   0x07    // light grey on black
#define ATTR_FOOTER 0x0e    // yellow on black

// Banner rows 5-7 with the heading on 6, body 9-20, footer 22 — roughly where
// a DOS box would put them, and clear of the 640x400 letterbox edges.
#define BANNER_TOP  5
#define BANNER_ROW  6
#define BANNER_END  7
#define BODY_TOP    9
#define BODY_BOTTOM 20
#define FOOTER_ROW  22
#define BODY_INDENT 4
#define BODY_WIDTH  (TXT_SCREEN_W - 2 * BODY_INDENT)

static void put_str(int row, int col, uint8_t attr, const char *s, int len)
{
    uint8_t *cell = text_screen_data + row * TXT_ROW_BYTES + col * 2;
    for (int i = 0; i < len && col + i < TXT_SCREEN_W; i++) {
        *cell++ = (uint8_t)s[i];
        *cell++ = attr;
    }
}

static void fill_row(int row, uint8_t attr)
{
    uint8_t *cell = text_screen_data + row * TXT_ROW_BYTES;
    for (int i = 0; i < TXT_SCREEN_W; i++) {
        *cell++ = ' ';
        *cell++ = attr;
    }
}

static void put_centered(int row, uint8_t attr, const char *s)
{
    int len = (int)strlen(s);
    if (len > TXT_SCREEN_W) len = TXT_SCREEN_W;
    put_str(row, (TXT_SCREEN_W - len) / 2, attr, s, len);
}

// Greedy word wrap into rows BODY_TOP..BODY_BOTTOM. '\n' forces a break (so
// "\n\n" leaves a blank line) and leading spaces after one are kept, which is
// how the callers indent example command lines. A word wider than the column
// is hard-split rather than dropped. Anything past BODY_BOTTOM is truncated —
// the UART copy printed by doom_error_screen() always has the whole message.
static void put_body(const char *body)
{
    int row = BODY_TOP;
    const char *p = body;
    boolean wrapped = false;    // got here by running out of columns, not by '\n'
    while (*p && row <= BODY_BOTTOM) {
        // The space we broke at would otherwise indent the continuation line.
        if (wrapped) {
            while (*p == ' ') p++;
        }
        if (*p == '\n') {
            p++;
            row++;
            wrapped = false;
            continue;
        }
        if (!*p) break;
        int take = 0, last_space = 0;
        while (p[take] && p[take] != '\n' && take < BODY_WIDTH) {
            if (p[take] == ' ') last_space = take;
            take++;
        }
        wrapped = p[take] && p[take] != '\n';
        if (wrapped && last_space) take = last_space;
        put_str(row++, BODY_INDENT, ATTR_BODY, p, take);
        p += take;
        if (*p == '\n') {
            p++;
            wrapped = false;
        }
    }
}

void doom_error_screen(const char *heading, const char *fmt, ...)
{
    char body[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);

    // UART first, and unconditionally: it costs nothing, keeps the boot log
    // self-explanatory, and is all we have if the HDMI sink is asleep.
    printf("\n*** %s ***\n%s\n", heading, body);

    text_screen_prepare();
    for (int row = BANNER_TOP; row <= BANNER_END; row++) {
        fill_row(row, ATTR_BANNER);
    }
    put_centered(BANNER_ROW, ATTR_BANNER, heading);
    put_body(body);
    put_centered(FOOTER_ROW, ATTR_FOOTER, "Press the RESET button to try again.");

    if (!text_screen_show()) {
        // Scanvideo build with the video pipeline not up yet — no way to show
        // the page we just composed, so behave as before.
        panic("%s: %s", heading, body);
    }
    while (true) {
        tight_loop_contents();
    }
}

#endif // PICO_ON_DEVICE
