#pragma once

#include "../Sim/Thinker.h"
#include "../Math/FixedPoint.h"

namespace Doom
{
struct Sector;

enum class FloorType
{
    // lower floor to highest surrounding floor
    LowerFloor,

    // lower floor to lowest surrounding floor
    LowerFloorToLowest,

    // lower floor to highest surrounding floor VERY FAST
    TurboLower,

    // raise floor to lowest surrounding CEILING
    RaiseFloor,

    // raise floor to next highest surrounding floor
    RaiseFloorToNearest,

    // raise floor to shortest height texture around it
    RaiseToTexture,

    // lower floor to lowest surrounding floor
    //  and change floorpic
    LowerAndChange,

    RaiseFloor24,
    RaiseFloor24AndChange,
    RaiseFloorCrush,

    // raise to next highest floor, turbo-speed
    RaiseFloorTurbo,
    DonutRaise,
    RaiseFloor512
};

enum class StairType
{
    Build8, // slowly build by 8
    Turbo16 // quickly build by 16
};

constexpr Fixed FLOORSPEED = FRACUNIT;

// A moving floor. It drives its sector's floor to floordestheight, optionally
// changing the sector's texture/special on arrival; the exact behaviour depends on
// its FloorType. Its per-tic behaviour is tick() (FloorMove.cpp); the EV_ handlers
// in Sim/Floors.cpp spawn it.
struct FloorMove : Thinker
{
    void tick() override;
    ThinkerKind kind() const override { return ThinkerKind::Floor; }
    FloorType type;
    bool crush;
    Sector* sector;
    int direction;
    int newspecial;
    short texture;
    Fixed floordestheight;
    Fixed speed;
};
} // namespace Doom
