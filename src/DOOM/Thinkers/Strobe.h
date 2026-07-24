#pragma once

#include "../Sim/Thinker.h"

namespace Doom
{
struct Sector;

// Strobe timing, in tics. STROBEBRIGHT is how long the bright phase lasts; a strobe
// is spawned fast (FASTDARK) or slow (SLOWDARK) for the dark phase. Used by the
// spawners and startLightStrobing (Sim/Lights.cpp); tick() reads the per-instance
// bright/darktime it seeds from these.
constexpr int STROBEBRIGHT = 5;
constexpr int FASTDARK = 15;
constexpr int SLOWDARK = 35;

// A sector light that strobes between minlight and maxlight, holding bright for
// brighttime and dark for darktime. Its per-tic behaviour is tick() (Strobe.cpp);
// spawnStrobeFlash (Sim/Lights.cpp) creates and seeds it.
struct Strobe : Thinker
{
    void tick() override;
    ThinkerKind kind() const override { return ThinkerKind::StrobeFlash; }
    Sector* sector;
    int count;
    int minlight;
    int maxlight;
    int darktime;
    int brighttime;
};
} // namespace Doom
