#pragma once

#include "../Sim/Thinker.h"
#include "../Math/FixedPoint.h"

namespace Doom
{
struct Sector;

enum class PlatState
{
    Up,
    Down,
    Waiting,
    InStasis
};

enum class PlatType
{
    PerpetualRaise,
    DownWaitUpStay,
    RaiseAndChange,
    RaiseToNearestAndChange,
    BlazeDWUS
};

constexpr int PLATWAIT = 3;
constexpr Fixed PLATSPEED = FRACUNIT;
constexpr int MAXPLATS = 30;

// An elevator platform. It raises/lowers its sector between low and high, waiting
// in between; the exact cycle depends on its PlatType. Its per-tic behaviour is
// tick() (Plat.cpp); the EV_ handlers in Sim/Plats.cpp spawn and stop it.
struct Plat : Thinker
{
    void tick() override;
    ThinkerKind kind() const override { return ThinkerKind::Plat; }
    Sector* sector;
    Fixed speed;
    Fixed low;
    Fixed high;
    int wait;
    int count;
    PlatState status;
    PlatState oldstatus;
    bool crush;
    int tag;
    PlatType type;
};
} // namespace Doom
