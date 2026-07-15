#pragma once

#include "../p_local.h"

namespace Doom
{
// The transient movement/collision scratch p_maputl builds and the clipping code
// reads back. Two clusters:
//
//   - P_PathTraverse's intercept list: the lines and things a trace crosses, the
//     pointer into it, the early-out flag PIT_AddLineIntercepts honours, and the
//     trace itself (the directed segment being walked). Rebuilt from scratch on
//     every traverse.
//   - P_LineOpening's vertical window: the gap a two-sided line leaves (opentop /
//     openbottom / openrange) plus the lower of the two floors (lowfloor). Written
//     as each line is contacted, read by the mover in p_map/p_enemy.
//
// None of it is hashed - a demo's world is mobj fields, not this scratch - so
// gathering it here is golden-neutral. The vanilla names (opentop, trace, ...) are
// references onto these members while p_map/p_sight/p_enemy still read them as
// globals; they resolve to clip().<member> directly once those files take an
// Engine&.
struct Clip
{
    intercept_t intercepts[MAXINTERCEPTS];
    intercept_t* interceptPtr = nullptr;
    doom_boolean earlyOut = false;

    // P_PathTraverse's trace, read back by the shooting code in p_map.
    divline_t trace = {};

    // P_LineOpening's window.
    fixed_t opentop = 0;
    fixed_t openbottom = 0;
    fixed_t openrange = 0;
    fixed_t lowfloor = 0;

    // P_CheckPosition / P_TryMove clipping state (vanilla's tm*). The mover being
    // clipped, its flags and centre, and the bounding box its radius sweeps.
    mobj_t* tmthing = nullptr;
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
    line_t* ceilingline = nullptr;

    // Special lines contacted during a move, held until the move is proven valid
    // and then crossed. Not sorted - two specials 8 units apart cross in either
    // order, a vanilla quirk.
    static constexpr int maxSpecialCross = 8;
    line_t* spechit[maxSpecialCross] = {};
    int numspechit = 0;

    // Hitscan results read outside the attack code: the thing a P_AimLineAttack
    // locked onto (0 if none), and the range of the shot in progress. p_mobj and
    // p_pspr read both; the rest of the attack scratch is file-local to MapAction.
    mobj_t* linetarget = nullptr;
    fixed_t attackrange = 0;
};

// The one Clip, a view onto the Engine's member - the same pattern as level(),
// wad() and randomness().
Clip& clip();
} // namespace Doom
