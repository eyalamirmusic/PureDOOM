#pragma once

#include "../d_player.h" // player_t, and fixed_t / angle_t through it.
#include "../m_fixed.h"
#include "../tables.h"

namespace Doom
{
// The camera the frame is drawn from. R_SetupFrame computes it once per frame from
// the view player - position, the angle it looks along, and the sin/cos of that
// angle - and the whole software renderer reads it back while it lays down the
// picture.
//
// It is the first scalar cluster to move off the loose globals into the Engine
// (REFACTOR.md, Step 5): the storage lives here, and the vanilla names
// (viewx, viewangle, ...) are references onto these members while r_main and the
// Render/ units still read them as globals. Nothing here is hashed - the simulation
// is mobj fields, and the frame goldens see the *picture*, not these numbers - so
// gathering it is golden-neutral, exactly as Random / Level / Clip were.
//
// viewz alone is read outside the renderer (the playsim's sound origin and the app's
// camera); the rest is the renderer's own. The references keep every one of those
// readers resolving unchanged until each file takes an Engine& of its own.
struct ViewPoint
{
    fixed_t viewx = 0;
    fixed_t viewy = 0;
    fixed_t viewz = 0;

    angle_t viewangle = 0;

    // sin/cos of viewangle, sampled from the fine tables alongside it.
    fixed_t viewcos = 0;
    fixed_t viewsin = 0;

    player_t* viewplayer = nullptr;
};

// The one ViewPoint, a view onto the Engine's member - the same pattern as clip(),
// level(), wad() and randomness().
ViewPoint& viewPoint();
} // namespace Doom
