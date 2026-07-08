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

extern "C" {
void pico_usb_key_down(int scancode, int shift);
void pico_usb_key_up(int scancode, int shift);
void pico_usb_post_joystick(int buttons, int xmove, int ymove, int strafemove);
#if !NO_USE_MOUSE
void pico_usb_post_mouse(int buttons, int dx, int dy, int wheel);
#endif
void pico_usb_hid_poll(void);
}

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

    // Merge all connected pads into one joystick event whose button bit
    // layout matches the joyb* defaults in m_controls.c:
    // 0 fire, 1 strafe, 2 speed/run, 3 use, 4 prev weapon, 5 next weapon.
    void pollGamePads()
    {
        using Button = io::GamePadState::Button;
        int buttons = 0, xmove = 0, ymove = 0, strafemove = 0;

        for (int p = 0; p < 2; p++)
        {
            const io::GamePadState &gp = io::getCurrentGamePadState(p);
            if (!gp.isConnected())
                continue;
            if (gp.buttons & Button::X) buttons |= 1 << 0;
            if (gp.buttons & Button::A) buttons |= 1 << 1;
            if (gp.buttons & Button::B) buttons |= 1 << 2;
            if (gp.buttons & Button::Y) buttons |= 1 << 3;
            if (gp.buttons & Button::SELECT) buttons |= 1 << 4;
            if (gp.buttons & Button::START) buttons |= 1 << 5;
            if (gp.buttons & Button::LEFT) xmove = -127;
            else if (gp.buttons & Button::RIGHT) xmove = 127;
            if (gp.buttons & Button::UP) ymove = -127;
            else if (gp.buttons & Button::DOWN) ymove = 127;
            if (gp.buttons & Button::L) strafemove = -127;
            else if (gp.buttons & Button::R) strafemove = 127;
        }

        static int pb, px, py, ps;
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
