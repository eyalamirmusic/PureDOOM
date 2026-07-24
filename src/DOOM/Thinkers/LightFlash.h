#pragma once

#include "../Sim/Thinker.h"

namespace Doom
{
struct Sector;

// A sector light that flashes on and off at random intervals: it toggles between
// maxlight and minlight, holding each for a random count bounded by maxtime/mintime.
// Its per-tic behaviour is tick() (LightFlash.cpp); spawnLightFlash (Sim/Lights.cpp)
// creates and seeds it.
struct LightFlash : Thinker
{
    void tick() override;
    ThinkerKind kind() const override { return ThinkerKind::LightFlash; }
    Sector* sector;
    int count;
    int maxlight;
    int minlight;
    int maxtime;
    int mintime;
};
} // namespace Doom
