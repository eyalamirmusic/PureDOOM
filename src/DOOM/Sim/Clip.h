#pragma once

#include "../p_local.h"

#include "MapGeometry.h" // DivLine

namespace Doom
{
// The transient movement/collision scratch Sim/MapUtil builds and the clipping
// code reads back. Two clusters:
//
//   - pathTraverse's intercept list: the lines and things a trace crosses, the
//     pointer into it, the early-out flag the line-intercept callback honours, and
//     the trace itself (the directed segment being walked). Rebuilt from scratch
//     on every traverse.
//   - updateLineOpening's vertical window: the gap a two-sided line leaves
//     (opentop / openbottom / openrange) plus the lower of the two floors
//     (lowfloor). Written as each line is contacted, read by the mover in
//     Sim/Movement, Sim/MapAction and Sim/Enemy.
//
// None of it is hashed - a demo's world is mobj fields, not this scratch - so
// gathering it here is golden-neutral. Every reader reaches it through clip()
// now; the vanilla-named references onto these members went with p_maputl.cpp.
struct Clip
{
    Intercept intercepts[MAXINTERCEPTS];
    Intercept* interceptPtr = nullptr;
    doom_boolean earlyOut = false;

    // pathTraverse's trace, read back by the shooting code in MapAction.
    DivLine trace = {};

    // updateLineOpening's window.
    fixed_t opentop = 0;
    fixed_t openbottom = 0;
    fixed_t openrange = 0;
    fixed_t lowfloor = 0;

    // Doom::checkPosition / Doom::tryMove clipping state (vanilla's tm*). The mover being
    // clipped, its flags and centre, and the bounding box its radius sweeps.
    Mobj* tmthing = nullptr;
    int tmflags = 0;
    fixed_t tmx = 0;
    fixed_t tmy = 0;
    fixed_t tmbbox[4] = {};

    // floatok: the move would fit if the mobj sat between tmfloorz and tmceilingz.
    doom_boolean floatok = false;

    // The floor/ceiling the contacted lines leave for the mover, and the lowest
    // floor under it (a dropoff a monster refuses to walk off).
    fixed_t tmfloorz = 0;
    fixed_t tmceilingz = 0;
    fixed_t tmdropoffz = 0;

    // The line that lowered the ceiling, kept so missiles don't explode against
    // sky-hack walls.
    Line* ceilingline = nullptr;

    // Special lines contacted during a move, held until the move is proven valid
    // and then crossed. Not sorted - two specials 8 units apart cross in either
    // order, a vanilla quirk.
    static constexpr int maxSpecialCross = 8;
    Line* spechit[maxSpecialCross] = {};
    int numspechit = 0;

    // Hitscan results read outside the attack code: the thing a Doom::aimLineAttack
    // locked onto (0 if none), and the range of the shot in progress. p_mobj and
    // p_pspr read both; the rest of the attack scratch is file-local to MapAction.
    Mobj* linetarget = nullptr;
    fixed_t attackrange = 0;

    // The slope window narrowed as a trace crosses two-sided lines. DOOM reuses one
    // pair for two jobs it never runs at once: the auto-aim in MapAction and the
    // line-of-sight check in Sight. Both write and read them here.
    fixed_t topslope = 0;
    fixed_t bottomslope = 0;
};

// The one Clip, a view onto the Engine's member - the same pattern as level(),
// wad() and randomness().
Clip& clip();
} // namespace Doom
