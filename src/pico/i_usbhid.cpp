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
//     Bridge between the pico_shared USB HID host driver (hid_app.cpp,
//     vendored under 3rdparty/pico_shared_drivers/usb_hid) and Doom's
//     event queue. hid_app.cpp owns the TinyUSB host callbacks and keeps
//     the latest keyboard / mouse / gamepad state; pico_usb_hid_poll()
//     is called from I_GetEvent() right after tuh_task() and translates
//     state changes into Doom events via the pico_usb_* functions
//     exported by i_input.c.
//

#if USB_SUPPORT

#include "tusb.h"
#include "gamepad.h"
// For `boolean` (menuactive, below). It is one byte on both sides: `bool` here
// in C++, `uint8_t` in the C that defines it.
#include "doomtype.h"

// Legacy NES/SNES pads on PIO (pico_shared nespad, vendored under
// 3rdparty/pico_shared_drivers/nespad). Pin macros come from the board's
// force-included cflags header; -1 (or absent) disables a port. Their state
// is merged into the same joystick event as the USB pads in pollGamePads().
#if defined(NES_PIN_CLK) && NES_PIN_CLK != -1
#define DOOM_NESPAD 1
#include "nespad.h"
#include "hardware/clocks.h"
#include "pico/time.h"
#include <stdio.h>
#else
#define DOOM_NESPAD 0
#endif
#if DOOM_NESPAD && defined(NES_PIN_CLK_1) && NES_PIN_CLK_1 != -1
#define DOOM_NESPAD_1 1
#else
#define DOOM_NESPAD_1 0
#endif

// Wii-extension pads on I2C (NES/SNES Classic Mini, Wii Classic Controller
// (Pro)), through the doom_wiipad.cpp glue. Same -1-disables convention on the
// pin macros, and the same merge into the joystick event below.
#if defined(WII_PIN_SDA) && WII_PIN_SDA >= 0 && \
    defined(WII_PIN_SCL) && WII_PIN_SCL >= 0
#define DOOM_WIIPAD 1
#include "doom_wiipad.h"
#else
#define DOOM_WIIPAD 0
#endif

extern "C" {
void pico_usb_key_down(int scancode, int shift);
void pico_usb_key_up(int scancode, int shift);
void pico_usb_post_joystick(int buttons, int xmove, int ymove, int strafemove);
#if !NO_USE_MOUSE
void pico_usb_post_mouse(int buttons, int dx, int dy, int wheel);
#endif
void pico_usb_hid_poll(void);
// Exit screen (see i_system.c). The pad drives it directly rather than through
// the event queue, because I_Quit never runs D_ProcessEvents.
extern int8_t at_exit_screen;
// Which button layout to build the joystick mask from (m_controls.c), and
// whether the menu is up (m_menu.c) - the NES layout needs both, see below.
extern int8_t nes_pad_scheme;
extern boolean menuactive;
}

#include "doom_boot.h"

namespace
{
    bool findKey(const io::KeyboardState &st, uint8_t code)
    {
        for (int i = 0; i < 6; i++)
        {
            if (st.keycode[i] == code)
                return true;
        }
        return false;
    }

    // Diff the current boot-keyboard state against the previously seen one
    // and synthesize key down/up events. HID usage codes double as SDL
    // scancodes, which is what TranslateKey() in i_input.c expects.
    void pollKeyboard()
    {
        static io::KeyboardState prev;
        const io::KeyboardState cur = io::getCurrentKeyboardState();
        const int shift = (cur.modifier & (KEYBOARD_MODIFIER_LEFTSHIFT |
                                           KEYBOARD_MODIFIER_RIGHTSHIFT)) ? 1 : 0;

        for (int i = 0; i < 6; i++)
        {
            uint8_t code = cur.keycode[i];
            if (code && !findKey(prev, code))
                pico_usb_key_down(code, shift);
            code = prev.keycode[i];
            if (code && !findKey(cur, code))
                pico_usb_key_up(code, shift);
        }

        // Modifier bit n corresponds to HID usage 0xE0 + n
        // (LeftControl, LeftShift, LeftAlt, LeftGUI, Right...).
        const uint8_t changed = cur.modifier ^ prev.modifier;
        for (int i = 0; i < 8; i++)
        {
            const uint8_t mask = 1u << i;
            if (changed & mask)
            {
                if (cur.modifier & mask)
                    pico_usb_key_down(0xE0 + i, shift);
                else
                    pico_usb_key_up(0xE0 + i, shift);
            }
        }
        prev = cur;
    }

#if DOOM_NESPAD
    // Lazy-init on first poll, then harvest the PIO read started on the
    // previous poll and immediately kick off the next one (~200 µs per
    // transfer, one poll cycle of input latency — negligible).
    //
    // Three hard-won robustness rules, learned on the adafruitdvisd bring-up
    // and on the exit screen:
    //  - After nespad_begin() the SM runs at a 1 MHz PIO clock, so its
    //    first instruction — "irq wait 0", set-flag-then-park — lands ~1 µs
    //    after enable. The 378 MHz core reaches nespad_read_start() first,
    //    the clear outruns the SM's set, and the release is lost: the SM
    //    parks forever and the next finish blocks for good. Wait out the
    //    race before the first start.
    //  - Never call nespad_read_finish() (blocking FIFO reads) unless
    //    nespad_read_ready() says data is waiting — a controller port must
    //    not be able to hang the game loop.
    //  - Never start a read sooner than MIN_POLL_US after the last one. The
    //    autopush fills the FIFO a few SM instructions *before* the SM gets
    //    back to its irq-wait park, so a caller that polls flat out hits the
    //    very same lost-release race: it sees the word land, clears the IRQ,
    //    and the SM sets the flag afterwards and never wakes. The game loop
    //    polls once a tic and never noticed, but exit_screen_pump() spins, so
    //    the pad died the moment the DOS prompt came up and START stopped
    //    working. Rate limiting here rather than in the pump keeps the fix
    //    with the rule it belongs to, whatever the caller does.
    constexpr uint64_t MIN_POLL_US = 1000;

    uint16_t nesButtons()
    {
        static bool inited = false, dead = false;
        static uint16_t last = 0;
        static uint64_t not_ready_since = 0;
        static uint64_t last_start = 0;
        if (dead)
            return 0;
        if (!inited)
        {
            inited = true;
            const uint32_t cpu_khz = clock_get_hz(clk_sys) / 1000;
            bool ok = nespad_begin(0, cpu_khz, NES_PIN_CLK, NES_PIN_DATA, NES_PIN_LAT, NES_PIO);
#if DOOM_NESPAD_1
            ok = nespad_begin(1, cpu_khz, NES_PIN_CLK_1, NES_PIN_DATA_1, NES_PIN_LAT_1, NES_PIO_1) && ok;
#endif
            if (!ok)
            {
                printf("!!! nespad init failed - NES/SNES pads disabled\n");
                dead = true;
                return 0;
            }
            // Let both SMs (1 MHz PIO clock) reach their irq-wait park
            // before the first release — see race note above.
            busy_wait_us(100);
            nespad_read_start();
            last_start = time_us_64();
            return 0;
        }
        const uint64_t now = time_us_64();
        if (now - last_start < MIN_POLL_US)
        {
            // Too soon to touch the SM at all — not even the ready check, so a
            // caller spinning on this cannot age out the timeout below either.
            return last;
        }
        if (!nespad_read_ready())
        {
            // Reads complete in ~200 µs, well inside MIN_POLL_US. Transiently
            // not-ready: keep the previous state. Never-ready means a dead
            // state machine — give up loudly after a second.
            if (not_ready_since == 0)
                not_ready_since = now;
            else if (now - not_ready_since > 1000000)
            {
                printf("!!! nespad read never completed - NES/SNES pads disabled\n");
                dead = true;
            }
            return last;
        }
        not_ready_since = 0;
        nespad_read_finish();
        last = nespad_states_ext[0] | nespad_states_ext[1];
        nespad_read_start();
        last_start = now;
        return last;
    }
#endif

    // Bit 6 is joybmenu in m_controls.c (START). Named because it is referenced
    // both when building the joystick mask and by the exit-screen handling
    // below, and the two must not drift apart.
    constexpr int JOYB_MENU_BIT = 6;

    // The four buttons of a plain NES pad, merged across every connected pad.
    // Collected separately from the SNES mask because in the NES layout what a
    // button means depends on whether the menu is open.
    enum {
        NESBTN_A      = 1 << 0,
        NESBTN_B      = 1 << 1,
        NESBTN_SELECT = 1 << 2,
        NESBTN_START  = 1 << 3,
    };

    // The NES layout (nes_pad_scheme != 0). Four buttons cannot cover fire,
    // strafe, use, weapons *and* the menu, so they do double duty:
    //
    //                 in game                     in a menu
    //   A             fire        (bit 0)         select   (bit 0, joybfire)
    //   B             strafe      (bit 1)         back     (bit 3, joybuse)
    //   START         use         (bit 3)         -
    //   SELECT        next weapon (bit 5)         -
    //   SELECT+START  menu        (bit 6)         menu     (bit 6)
    //
    // Emitting joybuse for B in menus is what lets m_menu.c stay untouched: it
    // already turns joybfire into forward/confirm and joybuse into back/abort.
    // There is no run button either, which is why the toggle also switches on
    // the vanilla autorun hack (see M_SetNesPadScheme in m_controls.c).
    int nesButtonMask(int nesbtn)
    {
        // The chord suppresses its own members until both are released, so
        // letting go of one of them cannot make the other look like a fresh
        // press and fire a stray Use or weapon switch.
        static bool chord_latched;
        static int8_t was_menuactive = -1;
        static int held_over;

        const int8_t in_menu = menuactive ? 1 : 0;

        // A button already down when the menu opens or closes would otherwise
        // immediately act out its new meaning: holding B (strafe) while
        // chording the menu open would make it joybuse on the very next poll
        // and close the menu again. Ignore whatever is held across the
        // transition until it is released. (Held buttons across a change of
        // layout are handled by pollGamePads, which has to cover both
        // directions.)
        if (was_menuactive != in_menu)
        {
            was_menuactive = in_menu;
            held_over = nesbtn;
        }
        held_over &= nesbtn;
        const int active = nesbtn & ~held_over;

        constexpr int chord = NESBTN_SELECT | NESBTN_START;
        const bool chord_now = (nesbtn & chord) == chord;
        int mask = 0;

        // Pulsed for a single poll rather than held: M_Responder turns any
        // event carrying joybmenu into key_menu_activate, and the mask changes
        // again as soon as menuactive flips, which with a held bit would toggle
        // the menu straight back.
        if (chord_now && !chord_latched)
            mask |= 1 << JOYB_MENU_BIT;
        if (chord_now)
            chord_latched = true;
        else if (!(nesbtn & chord))
            chord_latched = false;

        if (active & NESBTN_A)
            mask |= 1 << 0;                            // fire / select
        if (active & NESBTN_B)
            mask |= in_menu ? (1 << 3) : (1 << 1);     // back / strafe
        if (!in_menu && !chord_latched)
        {
            if (active & NESBTN_START)  mask |= 1 << 3; // use
            if (active & NESBTN_SELECT) mask |= 1 << 5; // next weapon
        }
        return mask;
    }

    // Merge all connected pads into one joystick event whose button bit
    // layout matches the joyb* defaults in m_controls.c:
    // 0 fire, 1 strafe, 2 speed/run, 3 use, 4 prev weapon, 5 next weapon,
    // 6 menu.
    //
    // START drives bit 6 (joybmenu) so the pad can open and close the menu
    // without a keyboard. That cost START its old next-weapon job, and as
    // every face button was already taken the two weapon cycles moved to the
    // shoulders (L/R), which is the usual console-Doom layout. SELECT stays a
    // second prev-weapon so the old muscle memory still works. The shoulders
    // therefore no longer drive strafemove directly — strafing is still there
    // the vanilla way, by holding the strafe button (A) and pushing left/right,
    // which g_game.c turns into sidestepping (see `strafe` at g_game.c:390).
    //
    // That layout wants six buttons and two shoulders, which a plain NES pad
    // does not have, so `legacy` is built alongside a four-button view of the
    // same pads and nes_pad_scheme picks between them.
    void pollGamePads()
    {
        using Button = io::GamePadState::Button;
        int legacy = 0, nesbtn = 0, xmove = 0, ymove = 0, strafemove = 0;

        for (int p = 0; p < 2; p++)
        {
            const io::GamePadState &gp = io::getCurrentGamePadState(p);
            if (!gp.isConnected())
                continue;
            if (gp.buttons & Button::X) legacy |= 1 << 0;
            if (gp.buttons & Button::A) legacy |= 1 << 1;
            if (gp.buttons & Button::B) legacy |= 1 << 2;
            if (gp.buttons & Button::Y) legacy |= 1 << 3;
            if (gp.buttons & (Button::SELECT | Button::L)) legacy |= 1 << 4;
            if (gp.buttons & Button::R) legacy |= 1 << 5;
            if (gp.buttons & Button::START) legacy |= 1 << JOYB_MENU_BIT;
            // A NES-shelled MantaPad reports its A on io A and its B on io X
            // (mode 2, see src/pico/CMakeLists.txt). Both face-button pairs are
            // accepted so the layout is also usable from a SNES-shelled pad.
            if (gp.buttons & (Button::A | Button::B)) nesbtn |= NESBTN_A;
            if (gp.buttons & (Button::X | Button::Y)) nesbtn |= NESBTN_B;
            if (gp.buttons & Button::SELECT) nesbtn |= NESBTN_SELECT;
            if (gp.buttons & Button::START) nesbtn |= NESBTN_START;
            if (gp.buttons & Button::LEFT) xmove = -127;
            else if (gp.buttons & Button::RIGHT) xmove = 127;
            if (gp.buttons & Button::UP) ymove = -127;
            else if (gp.buttons & Button::DOWN) ymove = 127;
        }

#if DOOM_NESPAD || DOOM_WIIPAD
        // One merged word for every "vintage" pad, in nespad's SNES serial
        // order: bit0=B, 1=Y, 2=Select, 3=Start, 4=Up, 5=Down, 6=Left,
        // 7=Right, 8=A, 9=X, 10=L, 11=R. Plain NES pads only populate bits
        // 0-7 (as A,B,Select,Start,dpad), so a NES pad gets A=fire / B=run.
        // Mapping mirrors the USB block above: primary=fire, secondary=run,
        // SNES A/X=strafe/use. A plain NES pad has no shoulders, so it trades
        // next-weapon for menu access on Start; Select still cycles weapons
        // the other way, which reaches all of them.
        //
        // wiipad_read() is OR-ed in unchanged, which is the whole point: its
        // low byte is *already* the NES pad layout (bit0=A, 1=B, 2=Select,
        // 3=Start, dpad), so a NES Classic Mini behaves exactly like an
        // original NES controller on a nespad port. The price is that its
        // bits 8-11 are X, Y, L, R where nespad has A, X, L, R, so an SNES
        // Classic Mini gets A=fire, B=run, X=strafe, Y=use rather than a real
        // SNES pad's B=fire, Y=run, A=strafe, X=use. The two Classic pads
        // cannot be told apart on the wire - they share the Wii Classic
        // Controller Pro protocol and identity block - so only one of them
        // can match its nespad equivalent, and the NES pad wins.
        uint16_t nes = 0;
#if DOOM_NESPAD
        nes |= nesButtons();
#endif
#if DOOM_WIIPAD
        nes |= doom_wiipad_read();
#endif
        if (nes & 0x0001) legacy |= 1 << 0;      // SNES B / NES A / wii A -> fire
        if (nes & 0x0100) legacy |= 1 << 1;      // SNES A / wii X -> strafe
        if (nes & 0x0002) legacy |= 1 << 2;      // SNES Y / NES B / wii B -> run
        if (nes & 0x0200) legacy |= 1 << 3;      // SNES X / wii Y -> use
        if (nes & 0x0404) legacy |= 1 << 4;      // Select / SNES L-> prev weapon
        if (nes & 0x0800) legacy |= 1 << 5;      // SNES R         -> next weapon
        if (nes & 0x0008) legacy |= 1 << JOYB_MENU_BIT; // Start   -> menu
        // Same pairing as the USB block: NES A is where SNES B and A sit, NES B
        // where SNES Y and X do, so a SNES pad on this port is usable in the
        // NES layout too.
        if (nes & 0x0101) nesbtn |= NESBTN_A;      // SNES B / NES A, SNES A
        if (nes & 0x0202) nesbtn |= NESBTN_B;      // SNES Y / NES B, SNES X
        if (nes & 0x0004) nesbtn |= NESBTN_SELECT;
        if (nes & 0x0008) nesbtn |= NESBTN_START;
        if (nes & 0x0040) xmove = -127;           // Left
        else if (nes & 0x0080) xmove = 127;       // Right
        if (nes & 0x0010) ymove = -127;           // Up
        else if (nes & 0x0020) ymove = 127;       // Down
#endif

        static bool prev_nes_scheme;
        static bool settling;
        const bool nes_scheme = nes_pad_scheme != 0;
        if (nes_scheme != prev_nes_scheme)
        {
            prev_nes_scheme = nes_scheme;
            settling = true;
        }

        int buttons = nes_scheme ? nesButtonMask(nesbtn) : legacy;

        // The only way to change layout is to press A on the menu item, so a
        // button is always still held at the switch - and it means different
        // things on either side of it. On a USB pad, io A is joybfire in the
        // NES layout but joybstrafe in the default one, and M_Responder takes
        // either as menu-forward, so the mask change alone activated the item a
        // second time and put the setting straight back. Report nothing until
        // every button has been let go. The d-pad is left alone: it navigates
        // and moves the same way in both layouts.
        if (settling)
        {
            if (buttons)
                buttons = 0;
            else
                settling = false;
        }

        static int pb, px, py, ps;

        if (at_exit_screen)
        {
            // I_Quit never pumps D_ProcessEvents, so posting a joystick event
            // here would go nowhere. I_Quit already put the A:\> prompt on
            // screen, so the only pad action left is START, which stands in for
            // typing DOOM at it. Read from `legacy` whatever the layout: there
            // is nothing else on that screen for a chord to disambiguate.
            const int newly = legacy & ~pb;
            pb = legacy; px = xmove; py = ymove; ps = strafemove;
            if (newly & (1 << JOYB_MENU_BIT))
            {
                doom_restart_game();
            }
            return;
        }

        if (buttons != pb || xmove != px || ymove != py || strafemove != ps)
        {
            pico_usb_post_joystick(buttons, xmove, ymove, strafemove);
            pb = buttons; px = xmove; py = ymove; ps = strafemove;
        }
    }

#if !NO_USE_MOUSE
    void pollMouse()
    {
        io::MouseState &m = io::getCurrentMouseState();
        static uint8_t prevButtons;

        const int dx = m.dx, dy = m.dy, wheel = m.wheel;
        m.dx = m.dy = m.wheel = 0;
        if (dx || dy || wheel || m.buttons != prevButtons)
        {
            pico_usb_post_mouse(m.buttons, dx, dy, wheel);
            prevButtons = m.buttons;
        }
    }
#endif
}

void pico_usb_hid_poll(void)
{
    pollKeyboard();
    pollGamePads();
#if !NO_USE_MOUSE
    pollMouse();
#endif
}

#endif // USB_SUPPORT
