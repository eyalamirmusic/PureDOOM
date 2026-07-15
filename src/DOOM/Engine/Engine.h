#pragma once

#include "../Render/ViewPoint.h"
#include "../Sim/Clip.h"
#include "../Sim/Level.h"
#include "../Sim/Random.h"
#include "../Wad/WadFile.h"

namespace Doom
{
// The engine's mutable state, gathered into one owner.
//
// Today it holds the three subsystems already rewritten out of the global cloud:
// the random sequence, the WAD, the level geometry. It grows as each further
// subsystem is rewritten to be a member rather than a scatter of globals
// (REFACTOR.md, Step 5) - doomstat's 73 externs, r_state's 44, p_local's 27.
//
// When the last of them has moved in, the engine can be *constructed* rather than
// only booted, and doom_init's inability to run twice stops mattering: a fresh
// Engine is a fresh world. Until then there is one instance (engine()), and the
// vanilla free functions randomness(), wad() and level() are views onto its
// members - so a caller that has been rewritten to take an Engine& and one that
// still reaches for the global see the same state.
struct Engine
{
    Random random;
    WadFile wad;
    Level level;
    Clip clip;
    ViewPoint viewPoint;
};

// The one instance, for as long as the vanilla globals still reach state by free
// function rather than through an Engine& they were handed.
Engine& engine();
} // namespace Doom
