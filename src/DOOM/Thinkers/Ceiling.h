#pragma once

#include "../Sim/Thinker.h"
#include "../Math/FixedPoint.h"

namespace Doom
{
struct Sector;

enum class CeilingType
{
    LowerToFloor,
    RaiseToHighest,
    LowerAndCrush,
    CrushAndRaise,
    FastCrushAndRaise,
    SilentCrushAndRaise
};

constexpr Fixed CEILSPEED = FRACUNIT;
constexpr int MAXCEILINGS = 30;

// A moving ceiling or crusher. It drives its sector's ceiling between
// bottomheight and topheight; the exact cycle (and whether it crushes) depends on
// its CeilingType. Its per-tic behaviour is tick() (Ceiling.cpp); the EV_ handlers
// in Sim/Ceilings.cpp spawn and stop it.
struct Ceiling : Thinker
{
    void tick() override;
    ThinkerKind kind() const override { return ThinkerKind::Ceiling; }
    CeilingType type;
    Sector* sector;
    Fixed bottomheight;
    Fixed topheight;
    Fixed speed;
    bool crush;

    // 1 = up, 0 = waiting, -1 = down
    int direction;

    // ID
    int tag;
    int olddirection;
};
} // namespace Doom
