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
// digit and punctuation key on Windows fall through to Doom::Key::Unknown, which
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
    Doom::Key doomKey;
};

inline constexpr DoomKeyMapping printableKeys[] = {

    {Graphics::KeyCode::A, Doom::Key::A},
    {Graphics::KeyCode::B, Doom::Key::B},
    {Graphics::KeyCode::C, Doom::Key::C},
    {Graphics::KeyCode::D, Doom::Key::D},
    {Graphics::KeyCode::E, Doom::Key::E},
    {Graphics::KeyCode::F, Doom::Key::F},
    {Graphics::KeyCode::G, Doom::Key::G},
    {Graphics::KeyCode::H, Doom::Key::H},
    {Graphics::KeyCode::I, Doom::Key::I},
    {Graphics::KeyCode::J, Doom::Key::J},
    {Graphics::KeyCode::K, Doom::Key::K},
    {Graphics::KeyCode::L, Doom::Key::L},
    {Graphics::KeyCode::M, Doom::Key::M},
    {Graphics::KeyCode::N, Doom::Key::N},
    {Graphics::KeyCode::O, Doom::Key::O},
    {Graphics::KeyCode::P, Doom::Key::P},
    {Graphics::KeyCode::Q, Doom::Key::Q},
    {Graphics::KeyCode::R, Doom::Key::R},
    {Graphics::KeyCode::S, Doom::Key::S},
    {Graphics::KeyCode::T, Doom::Key::T},
    {Graphics::KeyCode::U, Doom::Key::U},
    {Graphics::KeyCode::V, Doom::Key::V},
    {Graphics::KeyCode::W, Doom::Key::W},
    {Graphics::KeyCode::X, Doom::Key::X},
    {Graphics::KeyCode::Y, Doom::Key::Y},
    {Graphics::KeyCode::Z, Doom::Key::Z},

    {Graphics::KeyCode::Num0, Doom::Key::Digit0},
    {Graphics::KeyCode::Num1, Doom::Key::Digit1},
    {Graphics::KeyCode::Num2, Doom::Key::Digit2},
    {Graphics::KeyCode::Num3, Doom::Key::Digit3},
    {Graphics::KeyCode::Num4, Doom::Key::Digit4},
    {Graphics::KeyCode::Num5, Doom::Key::Digit5},
    {Graphics::KeyCode::Num6, Doom::Key::Digit6},
    {Graphics::KeyCode::Num7, Doom::Key::Digit7},
    {Graphics::KeyCode::Num8, Doom::Key::Digit8},
    {Graphics::KeyCode::Num9, Doom::Key::Digit9},

    {Graphics::KeyCode::Minus, Doom::Key::Minus},
    {Graphics::KeyCode::Equals, Doom::Key::Equals},
    {Graphics::KeyCode::LeftBracket, Doom::Key::LeftBracket},
    {Graphics::KeyCode::RightBracket, Doom::Key::RightBracket},
    {Graphics::KeyCode::Semicolon, Doom::Key::Semicolon},
    {Graphics::KeyCode::Quote, Doom::Key::Apostrophe},
    {Graphics::KeyCode::Comma, Doom::Key::Comma},
    {Graphics::KeyCode::Period, Doom::Key::Period},
    {Graphics::KeyCode::Slash, Doom::Key::Slash},
};

inline Doom::Key toDoomKey(const Graphics::KeyEvent& event)
{
    using namespace Graphics;

    // A key the Windows backend could not translate has no positional identity, so
    // it must not reach the character fallback: that would resolve it on key down
    // (where Windows reports characters) and not on key up, leaving it held down
    // forever. Dropping it is the safe answer, and DOOM binds nothing outside the
    // table below.
    if (event.keyCode == KeyCode::Unknown)
        return Doom::Key::Unknown;

    for (const auto& mapping: printableKeys)
        if (mapping.keyCode == event.keyCode)
            return mapping.doomKey;

    switch (event.keyCode)
    {
        case KeyCode::Tab:
            return Doom::Key::Tab;
        case KeyCode::Return:
            return Doom::Key::Enter;
        case KeyCode::Escape:
            return Doom::Key::Escape;
        case KeyCode::Space:
            return Doom::Key::Space;
        case KeyCode::Delete:
            return Doom::Key::Backspace;
        case KeyCode::LeftArrow:
            return Doom::Key::LeftArrow;
        case KeyCode::RightArrow:
            return Doom::Key::RightArrow;
        case KeyCode::UpArrow:
            return Doom::Key::UpArrow;
        case KeyCode::DownArrow:
            return Doom::Key::DownArrow;
        case KeyCode::F1:
            return Doom::Key::F1;
        case KeyCode::F2:
            return Doom::Key::F2;
        case KeyCode::F3:
            return Doom::Key::F3;
        case KeyCode::F4:
            return Doom::Key::F4;
        case KeyCode::F5:
            return Doom::Key::F5;
        case KeyCode::F6:
            return Doom::Key::F6;
        case KeyCode::F7:
            return Doom::Key::F7;
        case KeyCode::F8:
            return Doom::Key::F8;
        case KeyCode::F9:
            return Doom::Key::F9;
        case KeyCode::F10:
            return Doom::Key::F10;
        case KeyCode::F11:
            return Doom::Key::F11;
        case KeyCode::F12:
            return Doom::Key::F12;
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
            return (Doom::Key) character;
    }

    return Doom::Key::Unknown;
}

inline Doom::MouseButton toDoomButton(Graphics::MouseButton button)
{
    switch (button)
    {
        case Graphics::MouseButton::Right:
            return Doom::MouseButton::Right;
        case Graphics::MouseButton::Middle:
            return Doom::MouseButton::Middle;
        default:
            return Doom::MouseButton::Left;
    }
}
} // namespace PureDoom
