#pragma once

#include "Common.h"

#include <algorithm>
#include <bitset>

namespace PureDoom
{

// A controller drives the engine through the keys and the mouse it already
// reads, so nothing in the engine knows a pad exists: the sticks and buttons
// are turned into the DOOM keys they stand in for, held while the control is,
// and the right stick into mouse movement, which takes the same aim-ahead path
// the mouse does (View::pendingTurn).
//
// In a level the left stick walks and strafes (WASD, running past
// stickRunsAt), the right stick turns, the right trigger fires, the left
// trigger runs, South uses, the shoulders and West/North step through the
// weapons the player holds, Back is the automap and Start the menu, and the
// D-pad is the arrow keys. In the menu the D-pad and the left stick navigate,
// South is Enter - Y when a prompt wants an answer - and East is Escape.

// The DOOM keys a pad is holding down, one bit per key code, so one frame's
// set can be diffed against the last and only the changes posted.
using DoomKeySet = std::bitset<256>;

inline void holdKey(DoomKeySet& keys, Doom::Key key)
{
    if (key != Doom::Key::Unknown)
        keys.set((std::size_t) key & 0xff);
}

inline Doom::Key keyFromBit(std::size_t bit)
{
    return (Doom::Key) bit;
}

// A stick past this walks; past stickRunsAt it runs. The engine's movement is
// held keys at two speeds, so a stick is read as those two.
constexpr auto stickWalksAt = 0.3f;
constexpr auto stickRunsAt = 0.85f;

// A trigger past this is pressed.
constexpr auto triggerPressedAt = 0.5f;

// The right stick inside this turns nothing; outside it the deflection is
// squared, so a half push turns at a quarter rate and the aim is fine near
// the centre and fast at the edge.
constexpr auto aimDeadZone = 0.15f;

// Mouse units a second at full deflection, before the engine's own mouse
// sensitivity (the options menu's slider) scales them like any mouse movement:
// about 200 degrees a second at the default.
constexpr auto aimSpeed = 4500.0f;

// What the keys stand for depends on what is up: the same South button uses
// a door in a level and picks a menu item in the menu.
struct GamepadContext
{
    bool menu = false;
    bool prompt = false;
};

inline float squareKeepingSign(float value)
{
    return value * std::abs(value);
}

// The mouse movement the right stick stands for over `seconds`, in the units
// View::mouseMovement gathers: positive is a turn to the right.
inline float aimMovement(const Graphics::GamepadState& pad, double seconds)
{
    const auto x = pad.axis(Graphics::GamepadAxis::RightX);

    if (std::abs(x) < aimDeadZone)
        return 0.0f;

    const auto scaled = (std::abs(x) - aimDeadZone) / (1.0f - aimDeadZone);
    const auto curved = squareKeepingSign(x < 0.0f ? -scaled : scaled);

    return curved * aimSpeed * (float) seconds;
}

inline void holdMenuKeys(DoomKeySet& keys,
                         const Graphics::GamepadState& pad,
                         const GamepadContext& context)
{
    using Button = Graphics::GamepadButton;
    using Axis = Graphics::GamepadAxis;

    const auto x = pad.axis(Axis::LeftX);
    const auto y = pad.axis(Axis::LeftY);

    if (pad.isDown(Button::DpadUp) || y > stickWalksAt)
        holdKey(keys, Doom::Key::UpArrow);

    if (pad.isDown(Button::DpadDown) || y < -stickWalksAt)
        holdKey(keys, Doom::Key::DownArrow);

    if (pad.isDown(Button::DpadLeft) || x < -stickWalksAt)
        holdKey(keys, Doom::Key::LeftArrow);

    if (pad.isDown(Button::DpadRight) || x > stickWalksAt)
        holdKey(keys, Doom::Key::RightArrow);

    if (pad.isDown(Button::South))
        holdKey(keys, context.prompt ? Doom::Key::Y : Doom::Key::Enter);

    if (pad.isDown(Button::East) || pad.isDown(Button::Start))
        holdKey(keys, Doom::Key::Escape);
}

inline void holdGameKeys(DoomKeySet& keys, const Graphics::GamepadState& pad)
{
    using Button = Graphics::GamepadButton;
    using Axis = Graphics::GamepadAxis;

    const auto x = pad.axis(Axis::LeftX);
    const auto y = pad.axis(Axis::LeftY);

    if (pad.isDown(Button::DpadUp) || y > stickWalksAt)
        holdKey(keys, Doom::Key::W);

    if (pad.isDown(Button::DpadDown) || y < -stickWalksAt)
        holdKey(keys, Doom::Key::S);

    if (x < -stickWalksAt)
        holdKey(keys, Doom::Key::A);

    if (x > stickWalksAt)
        holdKey(keys, Doom::Key::D);

    if (pad.isDown(Button::DpadLeft))
        holdKey(keys, Doom::Key::LeftArrow);

    if (pad.isDown(Button::DpadRight))
        holdKey(keys, Doom::Key::RightArrow);

    if (std::max(std::abs(x), std::abs(y)) > stickRunsAt
        || pad.axis(Axis::LeftTrigger) > triggerPressedAt)
        holdKey(keys, Doom::Key::Shift);

    if (pad.axis(Axis::RightTrigger) > triggerPressedAt)
        holdKey(keys, Doom::Key::Ctrl);

    if (pad.isDown(Button::South))
        holdKey(keys, Doom::Key::Space);

    if (pad.isDown(Button::Start))
        holdKey(keys, Doom::Key::Escape);

    if (pad.isDown(Button::Back))
        holdKey(keys, Doom::Key::Tab);
}

// The keys the pad holds this frame. `weaponKey` is the digit a weapon step
// is holding, or Unknown (View decides it, since it needs the player's
// weapons).
inline DoomKeySet gamepadKeys(const Graphics::GamepadState& pad,
                              const GamepadContext& context,
                              Doom::Key weaponKey)
{
    auto keys = DoomKeySet {};

    if (context.menu)
    {
        holdMenuKeys(keys, pad, context);
        return keys;
    }

    holdGameKeys(keys, pad);
    holdKey(keys, weaponKey);
    return keys;
}

// The key that selects a weapon slot: the digits 1 to 7, as the keyboard has
// them. 0 is no slot.
inline Doom::Key weaponDigitKey(int slot)
{
    return slot <= 0 ? Doom::Key::Unknown : (Doom::Key) ('0' + slot);
}

// Which way a weapon step goes this frame, from the buttons that just went
// down: +1 for the next weapon, -1 for the previous, 0 for neither.
inline int weaponStep(const Graphics::GamepadState& pad)
{
    using Button = Graphics::GamepadButton;

    if (pad.wasPressed(Button::RightShoulder) || pad.wasPressed(Button::North))
        return 1;

    if (pad.wasPressed(Button::LeftShoulder) || pad.wasPressed(Button::West))
        return -1;

    return 0;
}

// Whether a weapon step's digit is still being held: the key stays down for
// as long as the button that chose it does, so the engine sees it on a tic.
inline bool weaponStepHeld(const Graphics::GamepadState& pad)
{
    using Button = Graphics::GamepadButton;

    return pad.isDown(Button::RightShoulder) || pad.isDown(Button::North)
           || pad.isDown(Button::LeftShoulder) || pad.isDown(Button::West);
}
} // namespace PureDoom
