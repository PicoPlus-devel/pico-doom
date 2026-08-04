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
//	The handful of options the tiny build lets you change, kept on the SD
//	card across power cycles. Stands in for m_config.c's default.cfg, which
//	is compiled out here (NO_USE_SAVE_CONFIG / NO_USE_BOUND_CONFIG).
//

#ifndef DOOM_SETTINGS_H
#define DOOM_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#if PICO_ON_DEVICE

// Read the settings file and apply it to the engine globals. Call before
// M_Init(), R_Init() and S_Init() — they latch the values — and after the SD
// card has been brought up. Missing, stale or damaged file: the compiled-in
// defaults stay, and nothing is written until something actually changes.
void doom_settings_load(void);

// Write the settings back if any of them differ from what is on the card.
// Cheap and silent when nothing changed (it compares against a shadow copy in
// RAM and never touches the card), so it can go anywhere convenient. Pauses
// the renderer and the sound around the write, so call it from the game/menu
// thread, not from an interrupt.
void doom_settings_flush(void);

#else

#define doom_settings_load() ((void)0)
#define doom_settings_flush() ((void)0)

#endif

#ifdef __cplusplus
}
#endif

#endif // DOOM_SETTINGS_H
