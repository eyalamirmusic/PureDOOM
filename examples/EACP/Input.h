#pragma once

#include "Common.h"

namespace PureDoom
{

// The keys, mapped from the *positional* KeyCode GameInput reports. It carries no
// characters at all, so this is the only path rather than the preferred one, and
// a key with no case here is simply not DOOM's.
//
// Positional means by physical key: on a non-US layout the letters are the
// US-positional ones (the physical WASD block) rather than what the layout prints
// on them. For movement that is the right answer and matches other ports; for
// typing a savegame name it is a known trade rather than a surprise.
struct DoomKeyMapping
{
    std::uint16_t keyCode = 0;
    Doom::Key doomKey = Doom::Key::Unknown;
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

inline Doom::Key toDoomKey(std::uint16_t keyCode)
{
    using namespace Graphics;

    for (const auto& mapping: printableKeys)
        if (mapping.keyCode == keyCode)
            return mapping.doomKey;

    switch (keyCode)
    {
        // DOOM binds the modifiers as ordinary keys (fire, run, strafe) and does
        // not tell left from right.
        case KeyCode::Shift:
        case KeyCode::RightShift:
            return Doom::Key::Shift;
        case KeyCode::Control:
        case KeyCode::RightControl:
            return Doom::Key::Ctrl;
        case KeyCode::Option:
        case KeyCode::RightOption:
            return Doom::Key::Alt;
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
            return Doom::Key::Unknown;
    }
}

// DOOM has three buttons; a fourth fires, as the left one does.
inline Doom::MouseButton toDoomButton(Graphics::MouseButton button)
{
    switch (button)
    {
        case Graphics::MouseButton::Right:
            return Doom::MouseButton::Right;
        case Graphics::MouseButton::Middle:
            return Doom::MouseButton::Middle;
        case Graphics::MouseButton::Left:
        case Graphics::MouseButton::Other:
            return Doom::MouseButton::Left;
    }

    return Doom::MouseButton::Left;
}
} // namespace PureDoom
