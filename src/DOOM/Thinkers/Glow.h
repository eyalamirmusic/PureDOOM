#pragma once

#include "../Sim/Thinker.h"

namespace Doom
{
struct Sector;

// How much a glowing light changes per tic.
constexpr int GLOWSPEED = 8;

// A sector light that glows: it ramps the light up and down by GLOWSPEED between
// minlight and maxlight, reversing at each bound. Its per-tic behaviour is tick()
// (Glow.cpp); spawnGlowingLight (Sim/Lights.cpp) creates and seeds it.
struct Glow : Thinker
{
    void tick() override;
    ThinkerKind kind() const override { return ThinkerKind::Glow; }
    Sector* sector;
    int minlight;
    int maxlight;
    int direction;
};
} // namespace Doom
