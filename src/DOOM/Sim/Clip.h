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
};

// The one Clip, a view onto the Engine's member - the same pattern as level(),
// wad() and randomness().
Clip& clip();
} // namespace Doom
