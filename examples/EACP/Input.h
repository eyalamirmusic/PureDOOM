#pragma once

#include "Common.h"

namespace PureDoom
{
inline doom_key_t toDoomKey(const Graphics::KeyEvent& event)
{
    using namespace Graphics;

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

    // DOOM's codes for letters, digits and punctuation are their ASCII values, so
    // everything else maps through the typed character - which also covers the
    // keys eacp's KeyCode table has no constant for yet (see the gap log).
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
