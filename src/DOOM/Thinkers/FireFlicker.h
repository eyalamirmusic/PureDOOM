#pragma once

#include "../Sim/Thinker.h"

namespace Doom
{
struct Sector;

// A sector light that flickers like a fire: every 4 tics it drops the light by a
// random multiple of 16, floored at minlight. Its per-tic behaviour is tick()
// (FireFlicker.cpp); spawnFireFlicker (Sim/Lights.cpp) creates and seeds it.
struct FireFlicker : Thinker
{
    void tick() override;
    ThinkerKind kind() const override { return ThinkerKind::FireFlicker; }
    Sector* sector;
    int count;
    int maxlight;
    int minlight;
};
} // namespace Doom
