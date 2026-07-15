#pragma once

#include "../p_mobj.h" // mobj_t
#include "../r_defs.h" // line_t

namespace Doom
{
// Teleport `thing` to the MT_TELEPORTMAN in a sector tagged by `line`, with fog and
// sound. Returns 1 if it teleported, 0 otherwise. p_telept.cpp keeps the vanilla
// name EV_Teleport as a shim (p_spec/p_switch call it). Golden-neutral; the demos
// walk teleporters.
int teleport(line_t* line, int side, mobj_t* thing);
} // namespace Doom
