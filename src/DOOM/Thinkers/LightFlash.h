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
    Sector* sector = nullptr;
    int count = 0;
    int maxlight = 0;
    int minlight = 0;
    int maxtime = 0;
    int mintime = 0;
};
} // namespace Doom
