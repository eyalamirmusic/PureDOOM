#pragma once

#include "Common.h"

namespace PureDoom
{
// The printable keys, mapped from the *positional* KeyCode rather than from the
// character the event carries.
//
// This has to be the primary path, not an optimisation: eacp reports a KeyCode on
// every platform, but fills in charactersIgnoringModifiers only on macOS - the
// Windows backend never assigns it, and assigns `characters` on key *down* alone
// (see the gap log). Reading the character first therefore made every letter,
// digit and punctuation key on Windows fall through to DOOM_KEY_UNKNOWN, which
// View::keyDown drops - so w/a/s/d did nothing while Space, Escape, Enter and the
// arrows, all of which have a KeyCode case below, worked. That looked like partial
// keyboard support and was really one missing string.
//
// KeyCode is positional (macOS virtual key values, which the Windows backend
// translates to), so this matches by physical key, which is what a movement
// binding wants. The character fallback below is kept for the keys eacp has no
// constant for.
//
// Positional has to win over the character even where both are available, and the
// reason is key *up*, not layout: Windows reports characters on key down alone. A
// mapping that read the character first would identify a key one way when pressed
// and another way when released, and on any layout where those disagree the
// release would not match the press - so the engine would never clear the key and
// the player would run into a wall forever. Down and up must resolve identically,
// which only the positional code does on both platforms.
//
// The cost is that on a non-US macOS layout the letters are now the US-positional
// ones (the physical WASD block), where before they followed the layout. For
// movement that is the better answer and matches what other ports do; for typing a
// savegame name it is a regression, and the note is here so that is a known trade
// rather than a surprise.
struct DoomKeyMapping
{
    std::uint16_t keyCode;
    doom_key_t doomKey;
};

inline constexpr DoomKeyMapping printableKeys[] = {
    {Graphics::KeyCode::A, DOOM_KEY_A},
    {Graphics::KeyCode::B, DOOM_KEY_B},
    {Graphics::KeyCode::C, DOOM_KEY_C},
    {Graphics::KeyCode::D, DOOM_KEY_D},
    {Graphics::KeyCode::E, DOOM_KEY_E},
    {Graphics::KeyCode::F, DOOM_KEY_F},
    {Graphics::KeyCode::G, DOOM_KEY_G},
    {Graphics::KeyCode::H, DOOM_KEY_H},
    {Graphics::KeyCode::I, DOOM_KEY_I},
    {Graphics::KeyCode::J, DOOM_KEY_J},
    {Graphics::KeyCode::K, DOOM_KEY_K},
    {Graphics::KeyCode::L, DOOM_KEY_L},
    {Graphics::KeyCode::M, DOOM_KEY_M},
    {Graphics::KeyCode::N, DOOM_KEY_N},
    {Graphics::KeyCode::O, DOOM_KEY_O},
    {Graphics::KeyCode::P, DOOM_KEY_P},
    {Graphics::KeyCode::Q, DOOM_KEY_Q},
    {Graphics::KeyCode::R, DOOM_KEY_R},
    {Graphics::KeyCode::S, DOOM_KEY_S},
    {Graphics::KeyCode::T, DOOM_KEY_T},
    {Graphics::KeyCode::U, DOOM_KEY_U},
    {Graphics::KeyCode::V, DOOM_KEY_V},
    {Graphics::KeyCode::W, DOOM_KEY_W},
    {Graphics::KeyCode::X, DOOM_KEY_X},
    {Graphics::KeyCode::Y, DOOM_KEY_Y},
    {Graphics::KeyCode::Z, DOOM_KEY_Z},

    {Graphics::KeyCode::Num0, DOOM_KEY_0},
    {Graphics::KeyCode::Num1, DOOM_KEY_1},
    {Graphics::KeyCode::Num2, DOOM_KEY_2},
    {Graphics::KeyCode::Num3, DOOM_KEY_3},
    {Graphics::KeyCode::Num4, DOOM_KEY_4},
    {Graphics::KeyCode::Num5, DOOM_KEY_5},
    {Graphics::KeyCode::Num6, DOOM_KEY_6},
    {Graphics::KeyCode::Num7, DOOM_KEY_7},
    {Graphics::KeyCode::Num8, DOOM_KEY_8},
    {Graphics::KeyCode::Num9, DOOM_KEY_9},

    {Graphics::KeyCode::Minus, DOOM_KEY_MINUS},
    {Graphics::KeyCode::Equals, DOOM_KEY_EQUALS},
    {Graphics::KeyCode::LeftBracket, DOOM_KEY_LEFT_BRACKET},
    {Graphics::KeyCode::RightBracket, DOOM_KEY_RIGHT_BRACKET},
    {Graphics::KeyCode::Semicolon, DOOM_KEY_SEMICOLON},
    {Graphics::KeyCode::Quote, DOOM_KEY_APOSTROPHE},
    {Graphics::KeyCode::Comma, DOOM_KEY_COMMA},
    {Graphics::KeyCode::Period, DOOM_KEY_PERIOD},
    {Graphics::KeyCode::Slash, DOOM_KEY_SLASH},
};

inline doom_key_t toDoomKey(const Graphics::KeyEvent& event)
{
    using namespace Graphics;

    // A key the Windows backend could not translate has no positional identity, so
    // it must not reach the character fallback: that would resolve it on key down
    // (where Windows reports characters) and not on key up, leaving it held down
    // forever. Dropping it is the safe answer, and DOOM binds nothing outside the
    // table below.
    if (event.keyCode == KeyCode::Unknown)
        return DOOM_KEY_UNKNOWN;

    for (const auto& mapping: printableKeys)
        if (mapping.keyCode == event.keyCode)
            return mapping.doomKey;

    switch (event.keyCode)
    {
        case KeyCode::Tab:
            return DOOM_KEY_TAB;
        case KeyCode::Return:
            return DOOM_KEY_ENTER;
        case KeyCode::Escape:
            return DOOM_KEY_ESCAPE;
        case KeyCode::Space:
            return DOOM_KEY_SPACE;
        case KeyCode::Delete:
            return DOOM_KEY_BACKSPACE;
        case KeyCode::LeftArrow:
            return DOOM_KEY_LEFT_ARROW;
        case KeyCode::RightArrow:
            return DOOM_KEY_RIGHT_ARROW;
        case KeyCode::UpArrow:
            return DOOM_KEY_UP_ARROW;
        case KeyCode::DownArrow:
            return DOOM_KEY_DOWN_ARROW;
        case KeyCode::F1:
            return DOOM_KEY_F1;
        case KeyCode::F2:
            return DOOM_KEY_F2;
        case KeyCode::F3:
            return DOOM_KEY_F3;
        case KeyCode::F4:
            return DOOM_KEY_F4;
        case KeyCode::F5:
            return DOOM_KEY_F5;
        case KeyCode::F6:
            return DOOM_KEY_F6;
        case KeyCode::F7:
            return DOOM_KEY_F7;
        case KeyCode::F8:
            return DOOM_KEY_F8;
        case KeyCode::F9:
            return DOOM_KEY_F9;
        case KeyCode::F10:
            return DOOM_KEY_F10;
        case KeyCode::F11:
            return DOOM_KEY_F11;
        case KeyCode::F12:
            return DOOM_KEY_F12;
        default:
            break;
    }

    // Last resort, for a key with no KeyCode constant at all. DOOM's codes for
    // letters, digits and punctuation are their ASCII values, so the typed
    // character is directly usable where one is reported. Only macOS reports one:
    // this yields nothing on Windows, which is why the table above exists.
    const auto& characters = event.charactersIgnoringModifiers;

    if (characters.size() == 1)
    {
        auto character = characters.front();

        if (character >= 'A' && character <= 'Z')
            character = (char) (character - 'A' + 'a');

        if (character >= ' ' && character <= '~')
            return (doom_key_t) character;
    }

    return DOOM_KEY_UNKNOWN;
}

inline doom_button_t toDoomButton(Graphics::MouseButton button)
{
    switch (button)
    {
        case Graphics::MouseButton::Right:
            return DOOM_RIGHT_BUTTON;
        case Graphics::MouseButton::Middle:
            return DOOM_MIDDLE_BUTTON;
        default:
            return DOOM_LEFT_BUTTON;
    }
}
} // namespace PureDoom
