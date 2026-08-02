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
//	Options that survive a power cycle, in /settings_DOOM.dat on the SD
//	card. The tiny build compiles out m_config.c's default.cfg handling
//	(NO_USE_SAVE_CONFIG / NO_USE_BOUND_CONFIG), so this stands in for it,
//	covering exactly the settings this port can actually change: the two
//	volumes, messages, screen size, mouse sensitivity, gamma, the FPS
//	overlay and the NES pad layout.
//
//	Format follows pico-infonesPlus' /settings_<TAG>.dat convention
//	(pico_shared/settings.cpp): a fixed-size record with a leading version,
//	written raw, and validated on read by exact size plus version match.
//	Anything else falls back to the compiled-in defaults.
//

#if PICO_ON_DEVICE

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "doomtype.h"
#include "picodoom.h"
#include "m_controls.h"

#include "doom_sdcard.h"
#include "doom_settings.h"

#define DOOM_SETTINGS_PATH    "/settings_DOOM.dat"
#define DOOM_SETTINGS_VERSION 2

// Version 1 is the same 16 bytes without the NES pad layout, and settings_gather
// has always zeroed the record, so the byte the field now occupies reads back as
// "off" out of every file version 1 ever wrote. Accepting it costs one comparison
// and saves everyone their volumes; the shadow keeps the version it read, so the
// next flush quietly rewrites the file as version 2.
#define DOOM_SETTINGS_VERSION_MIN 1

// The engine globals this covers. Declared here rather than pulled in from the
// doom/ headers — several of them (showMessages) have no header of their own
// and are extern'd at the point of use anyway (see hu_stuff.c, d_main.c).
extern int sfxVolume;
extern int musicVolume;
extern isb_int8_t showMessages;
extern isb_int8_t screenblocks;
extern isb_int8_t mouseSensitivity;
extern isb_int8_t usegamma;
#if USE_FPS
extern boolean show_fps;
#endif

// On-disk layout: fixed-width types only. isb_int8_t is `int` or `int8_t`
// depending on the build (doomtype.h), so it must never appear here.
typedef struct {
    uint16_t version;
    int8_t sfx_volume;         // 0..15
    int8_t music_volume;       // 0..15
    int8_t show_messages;      // 0..1
    int8_t screenblocks;       // 3..11
    int8_t mouse_sensitivity;  // 0..9
    int8_t usegamma;           // 0..4
    int8_t show_fps;           // 0..1
    int8_t nes_pad_scheme;     // 0..1
    int8_t reserved[6];        // zeroed; bump the version before using any
} doom_settings_t;

static_assert(sizeof(doom_settings_t) == 16, "settings record is an on-disk format");

// What we believe is currently in the file. `shadow_valid` starts false, so the
// first flush writes even if nothing was touched — that is what creates the
// file on a fresh card.
static doom_settings_t shadow;
static bool shadow_valid;

static int8_t clamp8(int value, int lo, int hi)
{
    if (value < lo) return (int8_t)lo;
    if (value > hi) return (int8_t)hi;
    return (int8_t)value;
}

static void settings_gather(doom_settings_t *rec)
{
    memset(rec, 0, sizeof(*rec));
    rec->version           = DOOM_SETTINGS_VERSION;
    rec->sfx_volume        = clamp8(sfxVolume, 0, 15);
    rec->music_volume      = clamp8(musicVolume, 0, 15);
    rec->show_messages     = clamp8(showMessages, 0, 1);
    rec->screenblocks      = clamp8(screenblocks, 3, 11);
    rec->mouse_sensitivity = clamp8(mouseSensitivity, 0, 9);
    rec->usegamma          = clamp8(usegamma, 0, 4);
#if USE_FPS
    rec->show_fps          = show_fps ? 1 : 0;
#endif
    rec->nes_pad_scheme    = nes_pad_scheme ? 1 : 0;
}

static void settings_apply(const doom_settings_t *rec)
{
    // Clamped on the way in as well as on the way out: a hand-edited or
    // half-written file must not be able to put the engine in a state its own
    // menus cannot reach.
    sfxVolume        = clamp8(rec->sfx_volume, 0, 15);
    musicVolume      = clamp8(rec->music_volume, 0, 15);
    showMessages     = clamp8(rec->show_messages, 0, 1);
    screenblocks     = clamp8(rec->screenblocks, 3, 11);
    mouseSensitivity = clamp8(rec->mouse_sensitivity, 0, 9);
    usegamma         = clamp8(rec->usegamma, 0, 4);
#if USE_FPS
    show_fps         = rec->show_fps ? 1 : 0;
#endif
    // Not a plain assignment: the NES layout has no run button, so the flag
    // drags joybspeed along with it.
    M_SetNesPadScheme(rec->nes_pad_scheme);
}

void doom_settings_load(void)
{
    doom_settings_t rec;
    uint32_t got = 0;

    if (!doom_sd_read_file(DOOM_SETTINGS_PATH, &rec, sizeof(rec), &got)) {
        printf("settings: no usable " DOOM_SETTINGS_PATH ", using defaults\n");
        return;
    }
    if (got != sizeof(rec) || rec.version < DOOM_SETTINGS_VERSION_MIN
            || rec.version > DOOM_SETTINGS_VERSION) {
        printf("settings: " DOOM_SETTINGS_PATH " is %u bytes version %u,"
               " expected %u/%u..%u - using defaults\n",
               (unsigned)got, (unsigned)rec.version,
               (unsigned)sizeof(rec), DOOM_SETTINGS_VERSION_MIN,
               DOOM_SETTINGS_VERSION);
        return;
    }

    settings_apply(&rec);

    // Shadow the file as it was read, not as it was applied: if a value had to
    // be clamped the two now differ, so the next flush quietly rewrites the file
    // in its corrected form.
    shadow = rec;
    shadow_valid = true;

    printf("settings: loaded sfx %d music %d messages %d size %d mouse %d"
           " gamma %d fps %d nespad %d\n",
           rec.sfx_volume, rec.music_volume, rec.show_messages,
           rec.screenblocks, rec.mouse_sensitivity, rec.usegamma, rec.show_fps,
           rec.nes_pad_scheme);
}

void doom_settings_flush(void)
{
    doom_settings_t rec;
    settings_gather(&rec);

    if (shadow_valid && !memcmp(&rec, &shadow, sizeof(rec))) {
        return; // unchanged: no card access, no pause, nothing to see
    }

    // Checked before the pause so a cardless board does not stutter every time
    // a menu closes. doom_sd_ready() rate-limits its own retries.
    if (!doom_sd_ready()) {
        return;
    }

    // Same protection the flash writes used to get: core 1 parked on the
    // "saving" frame and the sound faded out around the I/O.
    pd_start_save_pause();
    int fr = doom_sd_write_file(DOOM_SETTINGS_PATH, &rec, sizeof(rec));
    pd_end_save_pause();

    // The shadow is updated either way. A card that is full or write protected
    // would otherwise make every single menu close pause the game to fail
    // again; this way the error is logged once and the next attempt happens
    // when a setting actually changes.
    shadow = rec;
    shadow_valid = true;
    if (fr != 0) {
        printf("settings: could not write " DOOM_SETTINGS_PATH ": %s (%d)\n",
               doom_sd_strerror(fr), fr);
    }
}

#endif // PICO_ON_DEVICE
