#pragma once

#include "../p_local.h"

namespace Doom
{
// The transient state P_PathTraverse builds and then walks in order: the list of
// lines and things a trace crosses, the pointer into it, and the early-out flag
// PIT_AddLineIntercepts honours. It is rebuilt from scratch on every traverse and
// read by nothing outside p_maputl, so it is the first of the movement/clipping
// state to move into the Engine whole. It is never hashed - a demo's world is mobj
// fields, not this scratch list - so relocating it is golden-neutral.
struct Clip
{
    intercept_t intercepts[MAXINTERCEPTS];
    intercept_t* interceptPtr = nullptr;
    doom_boolean earlyOut = false;
};

// The one Clip, a view onto the Engine's member - the same pattern as level(),
// wad() and randomness().
Clip& clip();
} // namespace Doom
