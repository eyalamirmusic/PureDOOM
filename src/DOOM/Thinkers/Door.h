#pragma once

#include "../Sim/Thinker.h"
#include "../Math/FixedPoint.h"

namespace Doom
{
struct Sector;

enum class DoorType
{
    Normal,
    Close30ThenOpen,
    Close,
    Open,
    RaiseIn5Mins,
    BlazeRaise,
    BlazeOpen,
    BlazeClose
};

constexpr Fixed VDOORSPEED = FRACUNIT * 2;
constexpr int VDOORWAIT = 150;

// A vertical door. It drives its sector's ceiling up and down, optionally waiting
// at the top; the exact cycle depends on its DoorType. Its per-tic behaviour is
// tick() (Door.cpp); the EV_ handlers in Sim/Doors.cpp spawn it.
struct Door : Thinker
{
    void tick() override;
    ThinkerKind kind() const override { return ThinkerKind::Door; }
    DoorType type;
    Sector* sector;
    Fixed topheight;
    Fixed speed;

    // 1 = up, 0 = waiting at top, -1 = down
    int direction;

    // tics to wait at the top
    int topwait;

    // (keep in case a door going down is reset)
    // when it reaches 0, start going down
    int topcountdown;
};
} // namespace Doom
