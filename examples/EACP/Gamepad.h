#pragma once

#include "Common.h"

#include <algorithm>
#include <bitset>

namespace PureDoom
{

// A controller drives the engine through three inputs it already reads. The
// buttons are turned into the DOOM keys they stand in for, held while the
// control is; the right stick into mouse movement, which takes the same
// aim-ahead path the mouse does (View::pendingTurn); and the left stick into
// the engine's joystick, which is analog, so a half push walks at half pace.
//
// In a level the left stick walks and strafes, the right stick turns, the
// right trigger fires, the left trigger runs (Shift, as on the keyboard),
// South uses, the shoulders and West/North step through the weapons the
// player holds, Back is the automap and Start the menu, and the D-pad is
// forward, back and the turn keys. In the menu the D-pad and the left stick
// navigate, South is Enter - Y when a prompt wants an answer - and East is
// Escape.

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

// The left stick inside this moves nothing, and in the menu is not a press of
// an arrow; outside it the deflection is rescaled to start from zero.
constexpr auto moveDeadZone = 0.2f;

// A trigger past this is pressed.
constexpr auto triggerPressedAt = 0.5f;

// The right stick inside this turns nothing; outside it the rate climbs
// faster than the push does, so the aim is fine near the centre and fast at
// the edge: a half push turns at three eighths of the full rate.
constexpr auto aimDeadZone = 0.15f;

// Mouse units a second at full deflection, before the engine's own mouse
// sensitivity (the options menu's slider) scales them like any mouse movement.
// About 265 degrees a second at the default, a little over the keyboard's
// running turn.
constexpr auto aimSpeed = 6000.0f;

// The engine's joystick range, Doom::joystickMove's units.
constexpr auto stickRange = 127;

// What the keys stand for depends on what is up: the same South button uses
// a door in a level and picks a menu item in the menu.
struct GamepadContext
{
    bool menu = false;
    bool prompt = false;
};

// A deflection with the dead zone taken off and the rest stretched back to
// 0..1, keeping its sign.
inline float pastDeadZone(float value, float deadZone)
{
    const auto size = std::abs(value);

    if (size < deadZone)
        return 0.0f;

    const auto scaled = std::min((size - deadZone) / (1.0f - deadZone), 1.0f);
    return value < 0.0f ? -scaled : scaled;
}

// The mouse movement the right stick stands for over `seconds`, in the units
// View::mouseMovement gathers: positive is a turn to the right.
inline float aimMovement(const Graphics::GamepadState& pad, double seconds)
{
    const auto push =
        pastDeadZone(pad.axis(Graphics::GamepadAxis::RightX), aimDeadZone);
    const auto curved = push * (0.5f + 0.5f * std::abs(push));

    return curved * aimSpeed * (float) seconds;
}

// The left stick as the engine's joystick: x strafing and y walking, each in
// -stickRange..stickRange with y up, zero inside the dead zone.
struct StickMove
{
    int x = 0;
    int y = 0;

    bool operator==(const StickMove&) const = default;
};

inline StickMove moveStick(const Graphics::GamepadState& pad)
{
    using Axis = Graphics::GamepadAxis;

    const auto x = pastDeadZone(pad.axis(Axis::LeftX), moveDeadZone);
    const auto y = pastDeadZone(pad.axis(Axis::LeftY), moveDeadZone);

    return {(int) std::lround(x * stickRange), (int) std::lround(y * stickRange)};
}

inline void holdMenuKeys(DoomKeySet& keys,
                         const Graphics::GamepadState& pad,
                         const GamepadContext& context)
{
    using Button = Graphics::GamepadButton;
    using Axis = Graphics::GamepadAxis;

    const auto x = pastDeadZone(pad.axis(Axis::LeftX), moveDeadZone);
    const auto y = pastDeadZone(pad.axis(Axis::LeftY), moveDeadZone);

    if (pad.isDown(Button::DpadUp) || y > 0.0f)
        holdKey(keys, Doom::Key::UpArrow);

    if (pad.isDown(Button::DpadDown) || y < 0.0f)
        holdKey(keys, Doom::Key::DownArrow);

    if (pad.isDown(Button::DpadLeft) || x < 0.0f)
        holdKey(keys, Doom::Key::LeftArrow);

    if (pad.isDown(Button::DpadRight) || x > 0.0f)
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

    if (pad.isDown(Button::DpadUp))
        holdKey(keys, Doom::Key::W);

    if (pad.isDown(Button::DpadDown))
        holdKey(keys, Doom::Key::S);

    if (pad.isDown(Button::DpadLeft))
        holdKey(keys, Doom::Key::LeftArrow);

    if (pad.isDown(Button::DpadRight))
        holdKey(keys, Doom::Key::RightArrow);

    if (pad.axis(Axis::LeftTrigger) > triggerPressedAt)
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
